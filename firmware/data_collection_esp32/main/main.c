/* Data Collection Firmware - ESP32-S3
 *
 * Reads BME280 (temperature, humidity, pressure via I2C) and
 * MAX31865 (PT100 RTD temperature via SPI), then outputs CSV
 * formatted data over the default serial port (UART0/USB-CDC).
 *
 * Also publishes sensor data via MQTT (WiFi / TLS) for remote
 * monitoring, and subscribes to command topics for remote control.
 *
 * Uses esp-idf-lib/max31865 driver for MAX31865 communication
 * (same driver as the working max31865_test demo).
 *
 * I2C pins:  SDA=GPIO22, SCL=GPIO21
 * SPI pins:  MOSI=GPIO23, MISO=GPIO19, SCLK=GPIO18, CS=GPIO15
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "max31865.h"
#include "bme280.h"

#if CONFIG_DC_LCD_ENABLE
#include "display.h"
#endif

/* ── WiFi credentials (from Kconfig / menuconfig) ──────────── */
#define WIFI_SSID           CONFIG_DC_WIFI_SSID
#define WIFI_PASSWORD       CONFIG_DC_WIFI_PASSWORD
#define WIFI_MAX_RETRY      CONFIG_DC_WIFI_MAX_RETRY

/* ── MQTT config (from Kconfig / menuconfig) ───────────────── */
#define MQTT_BROKER_URI     CONFIG_DC_MQTT_BROKER_URI
#define MQTT_CLIENT_ID      CONFIG_DC_MQTT_CLIENT_ID
#define MQTT_PUB_TOPIC      CONFIG_DC_MQTT_PUB_TOPIC
#define MQTT_SUB_TOPIC      CONFIG_DC_MQTT_SUB_TOPIC

/* ── MQTT publish interval (ms) ──────────────────────────────── */
#define MQTT_FAST_MS        100    /* cooling experiment: 10 Hz  */
#define MQTT_IDLE_MS        10000  /* standby:           0.1 Hz  */

/* ── I2C (BME280) ─────────────────────────────────────────────── */
#define I2C_MASTER_SCL_IO     21
#define I2C_MASTER_SDA_IO     22
#define I2C_MASTER_FREQ_HZ    100000

/* ── SPI (MAX31865) pins ──────────────────────────────────────── */
#define PIN_MOSI              CONFIG_DC_SPI_MOSI
#define PIN_MISO              CONFIG_DC_SPI_MISO
#define PIN_SCLK              CONFIG_DC_SPI_SCLK
#define PIN_CS                CONFIG_DC_SPI_CS

/* ── RTD parameters ────────────────────────────────────────────── */
#define RTD_NOMINAL            ((float)CONFIG_DC_RTD_NOMINAL)  /* PT100 = 100Ω */
#define RTD_REF                ((float)CONFIG_DC_RTD_REF)      /* Reference resistor on MAX31865 module board */

static const char *TAG = "data_collect";

/* ── Embedded TLS certificate (broker.emqx.io CA) ─────────────── */
extern const uint8_t broker_emqx_io_ca_crt_start[] asm("_binary_broker_emqx_io_ca_crt_start");
extern const uint8_t broker_emqx_io_ca_crt_end[]   asm("_binary_broker_emqx_io_ca_crt_end");

/* ── MQTT / WiFi globals ──────────────────────────────────────── */
static esp_mqtt_client_handle_t s_mqtt_client    = NULL;
static uint8_t                   s_wifi_retry     = 0;
static bool                      s_mqtt_fast_mode = true;   /* default: 10 Hz publishing */

/* ── Forward declarations ─────────────────────────────────────── */
static void mqtt_app_start(void);

/* ── I2C / BME280 globals ──────────────────────────────────────── */
static i2c_master_bus_handle_t  i2c_bus;
static i2c_master_dev_handle_t  bme280_dev;
static bme280_handle_t          bme280;

/* ================================================================
 *  I2C init
 * ================================================================ */
static void i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port   = -1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BME280_I2C_ADDRESS_DEFAULT,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &bme280_dev));
    ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

/* ================================================================
 *  CSV output helpers
 * ================================================================ */
static void print_csv_header(void)
{
    printf("PT100_C,BME280_T_C,Humidity_%%,Pressure_hPa\n");
}

/* ================================================================
 *  MQTT — publish sensor data as JSON
 * ================================================================ */
static void mqtt_publish_sensor_data(float pt100, float bme280_t,
                                     float humi, float pres)
{
    if (!s_mqtt_client) return;

    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddNumberToObject(root, "pt100_c",    roundf(pt100 * 100) / 100.0f);
    cJSON_AddNumberToObject(root, "bme280_c",   roundf(bme280_t * 100) / 100.0f);
    cJSON_AddNumberToObject(root, "humidity",   roundf(humi * 100) / 100.0f);
    cJSON_AddNumberToObject(root, "pressure",   roundf(pres * 100) / 100.0f);
    cJSON_AddBoolToObject(root, "fast_mode",    s_mqtt_fast_mode);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        esp_mqtt_client_publish(s_mqtt_client, MQTT_PUB_TOPIC,
                                json_str, 0, 1, 0);
        free(json_str);
    }
    cJSON_Delete(root);
}

/* ================================================================
 *  MQTT — event handler
 * ================================================================ */
static void mqtt_event_handler(void *args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event  = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected to %s", MQTT_BROKER_URI);
        esp_mqtt_client_subscribe(client, MQTT_SUB_TOPIC, 1);
        ESP_LOGI(TAG, "Subscribed to %s", MQTT_SUB_TOPIC);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribe ack, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        /* silently ack */
        break;

    case MQTT_EVENT_DATA: {
        char topic_buf[128];
        char data_buf[256];
        int tlen = event->topic_len < sizeof(topic_buf)-1
                       ? event->topic_len : (int)sizeof(topic_buf)-1;
        int dlen = event->data_len < sizeof(data_buf)-1
                       ? event->data_len : (int)sizeof(data_buf)-1;
        memcpy(topic_buf, event->topic, tlen);
        topic_buf[tlen] = '\0';
        memcpy(data_buf, event->data, dlen);
        data_buf[dlen] = '\0';

        ESP_LOGI(TAG, "MQTT cmd received [%s]: %s", topic_buf, data_buf);

        /* ── simple command parsing ─────────────────────────── */
        if (strstr(data_buf, "fast")) {
            s_mqtt_fast_mode = true;
            ESP_LOGI(TAG, "MQTT mode: fast (10 Hz)");
        } else if (strstr(data_buf, "idle")) {
            s_mqtt_fast_mode = false;
            ESP_LOGI(TAG, "MQTT mode: idle (0.1 Hz)");
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error: type=%d", event->error_handle->error_type);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "esp-tls err=0x%x, sock_errno=%d",
                     event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_transport_sock_errno);
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 *  MQTT — start client
 * ================================================================ */
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri              = MQTT_BROKER_URI,
        .broker.verification.certificate = (const char *)broker_emqx_io_ca_crt_start,
        .credentials.client_id           = MQTT_CLIENT_ID,
    };
    s_mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
    ESP_LOGI(TAG, "MQTT client starting...");
}

/* ================================================================
 *  WiFi — event handler
 * ================================================================ */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry < WIFI_MAX_RETRY) {
            s_wifi_retry++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d",
                     s_wifi_retry, WIFI_MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "WiFi connect failed after %d retries", WIFI_MAX_RETRY);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry = 0;
        ip_event_got_ip_t *info = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&info->ip_info.ip));
        mqtt_app_start();
    }
}

/* ================================================================
 *  WiFi — STA init
 * ================================================================ */
static void wifi_sta_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_START,
        wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
        wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        wifi_event_handler, NULL, NULL));

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "WiFi STA init done (SSID: %s)", WIFI_SSID);
}

/* ================================================================
 *  Sensor task
 * ================================================================ */
static void sensor_task(void *arg)
{
    /* ===========================================================
     * MAX31865 init — matches max31865_test demo pattern
     * =========================================================== */

    /* Step 1: CS pin init — pull HIGH before SPI bus takes over */
    gpio_config_t cs_conf = {
        .pin_bit_mask = 1ULL << PIN_CS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_conf));
    ESP_ERROR_CHECK(gpio_set_level(PIN_CS, 1));
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Step 2: SPI bus init */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, 1));
    ESP_LOGI(TAG, "SPI bus init: MOSI=%d MISO=%d SCLK=%d CS=%d",
             PIN_MOSI, PIN_MISO, PIN_SCLK, PIN_CS);

    /* Step 3: Init device descriptor (matches demo exactly) */
    max31865_t dev = {
        .standard = MAX31865_ITS90,
        .r_ref = RTD_REF,
        .rtd_nominal = RTD_NOMINAL,
    };
    ESP_ERROR_CHECK(max31865_init_desc(&dev, SPI2_HOST, MAX31865_MAX_CLOCK_SPEED_HZ, PIN_CS));

    /* Step 4: Configure device (3-wire, single-shot, 50Hz filter) */
    max31865_config_t config = {
        .v_bias = true,
        .filter = MAX31865_FILTER_50HZ,
        .mode = MAX31865_MODE_SINGLE,
        .connection = MAX31865_3WIRE,
    };
    ESP_ERROR_CHECK(max31865_set_config(&dev, &config));

    /* Step 5: Read back config to verify */
    max31865_config_t read_cfg = {0};
    esp_err_t cfg_res = max31865_get_config(&dev, &read_cfg);
    if (cfg_res == ESP_OK) {
        ESP_LOGI(TAG, "MAX31865 config readback: v_bias=%d filter=%d mode=%d connection=%d",
                 read_cfg.v_bias, read_cfg.filter, read_cfg.mode, read_cfg.connection);
    } else {
        ESP_LOGE(TAG, "MAX31865 config readback failed: %d (%s)", cfg_res, esp_err_to_name(cfg_res));
    }

    /* ===========================================================
     * BME280 init
     * =========================================================== */
    i2c_init();
    bme280 = bme280_create(bme280_dev);
    if (bme280_default_init(bme280) != ESP_OK) {
        ESP_LOGE(TAG, "BME280 init failed!");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "BME280 initialized");

    /* Discard first reading */
    float discard;
    bme280_read_temperature(bme280, &discard);
    bme280_read_humidity(bme280, &discard);
    bme280_read_pressure(bme280, &discard);

    /* Print CSV header */
    print_csv_header();

    /* ── LCD display init (optional, safe without screen) ── */
#if CONFIG_DC_LCD_ENABLE
    bool lcd_ok = display_init();
    if (lcd_ok) {
        ESP_LOGI(TAG, "LCD display active");
    } else {
        ESP_LOGW(TAG, "LCD not available — serial output only");
    }
#endif

    /* ===========================================================
     * Main loop
     * =========================================================== */
    uint32_t mqtt_last_ms = 0;

    while (1) {
        float pt100_temp = 0.0f;
        float bme280_temp = 0.0f;
        float humidity = 0.0f;
        float pressure = 0.0f;

        /* Read PT100 (matches demo's max31865_measure pattern) */
        esp_err_t res = max31865_measure(&dev, &pt100_temp);
        if (res != ESP_OK) {
            uint8_t fault_status = 0;
            ESP_LOGW(TAG, "MAX31865 measure failed: %d (%s)", res, esp_err_to_name(res));
            if (max31865_get_fault_status(&dev, &fault_status) == ESP_OK)
                ESP_LOGW(TAG, "MAX31865 fault status: 0x%02X", fault_status);
            uint16_t raw = 0;
            bool fault = false;
            if (max31865_read_raw(&dev, &raw, &fault) == ESP_OK) {
                float r_rtd = raw * RTD_REF / 32768.0f;
                ESP_LOGW(TAG, "RTD raw: %u, fault: %d, r_rtd: %.4f", raw, fault, r_rtd);
            }
            max31865_clear_fault_status(&dev);
            max31865_set_config(&dev, &config);
            pt100_temp = -999.0f;
        }

        /* Read BME280 */
        if (bme280_read_temperature(bme280, &bme280_temp) != ESP_OK)
            bme280_temp = -999.0f;
        if (bme280_read_humidity(bme280, &humidity) != ESP_OK)
            humidity = -999.0f;
        if (bme280_read_pressure(bme280, &pressure) != ESP_OK)
            pressure = -999.0f;

        /* Output CSV line (always, independent of MQTT) */
        printf("%.2f,%.2f,%.2f,%.2f\n",
               pt100_temp, bme280_temp, humidity, pressure);

        /* ── MQTT publish (rate-limited) ────────────────────── */
        {
            uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t interval = s_mqtt_fast_mode ? MQTT_FAST_MS : MQTT_IDLE_MS;
            if (now_ms - mqtt_last_ms >= interval) {
                mqtt_last_ms = now_ms;
                mqtt_publish_sensor_data(pt100_temp, bme280_temp,
                                         humidity, pressure);
            }
        }

        /* Update LCD display (every 8 cycles = 200ms) */
#if CONFIG_DC_LCD_ENABLE
        {
            static int tick = 0;
            if (++tick >= 8) {
                tick = 0;
                display_set_pt100_temperature(pt100_temp);
                display_set_bme280_temperature(bme280_temp);
                display_set_humidity(humidity);
                display_set_pressure(pressure);
            }
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

/* ================================================================
 *  Entry point
 * ================================================================ */
void app_main(void)
{
    /* ── NVS init (required by WiFi) ─────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ── Event loop (required by WiFi + MQTT) ────────────────── */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* ── WiFi STA (MQTT starts automatically on connect) ─────── */
    wifi_sta_init();

    /* ── Sensor task ──────────────────────────────────────────── */
    xTaskCreate(sensor_task, "sensor_task", 8192, NULL, 5, NULL);
}
