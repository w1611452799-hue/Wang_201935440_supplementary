#include <stdio.h>
#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>

#define HOST SPI2_HOST
#define MAX31865_MAX_CLOCK_SPEED_HZ 1000000

#define REG_CONFIG         0x00
#define REG_RTD_MSB        0x01
#define REG_FAULT_STATUS   0x07

#define BIT_CONFIG_50HZ        BIT(0)
#define BIT_CONFIG_FAULT_CLEAR BIT(1)
#define BIT_CONFIG_FAULT_D2    BIT(2)
#define BIT_CONFIG_FAULT_D3    BIT(3)
#define BIT_CONFIG_3WIRE       BIT(4)
#define BIT_CONFIG_1SHOT       BIT(5)
#define BIT_CONFIG_AUTO        BIT(6)
#define BIT_CONFIG_VBIAS       BIT(7)

#if CONFIG_EXAMPLE_FILTER_50
#define FILTER_50HZ 1
#else
#define FILTER_50HZ 0
#endif

static const char *TAG = "max31865-manual";
static spi_device_handle_t spi_dev;

static esp_err_t spi_write_reg(uint8_t reg, uint8_t val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = { (uint8_t)(reg | 0x80), val };
    t.tx_buffer = tx;
    t.length = sizeof(tx) * 8;
    return spi_device_transmit(spi_dev, &t);
}

static esp_err_t spi_read_reg8(uint8_t reg, uint8_t *val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = { reg, 0x00 };
    uint8_t rx[sizeof(tx)];
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    t.length = sizeof(tx) * 8;
    esp_err_t res = spi_device_transmit(spi_dev, &t);
    if (res != ESP_OK)
        return res;
    *val = rx[1];
    return ESP_OK;
}

static esp_err_t spi_read_reg16(uint8_t reg, uint16_t *val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = { reg, 0x00, 0x00 };
    uint8_t rx[sizeof(tx)];
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    t.length = sizeof(tx) * 8;
    esp_err_t res = spi_device_transmit(spi_dev, &t);
    if (res != ESP_OK)
        return res;
    *val = ((uint16_t)rx[1] << 8) | rx[2];
    return ESP_OK;
}

static esp_err_t max31865_write_config(void)
{
    uint8_t val = 0;
    val |= BIT_CONFIG_VBIAS;
    if (FILTER_50HZ)
        val |= BIT_CONFIG_50HZ;
    val |= BIT_CONFIG_3WIRE;
    return spi_write_reg(REG_CONFIG, val);
}

static esp_err_t max31865_start_one_shot(void)
{
    uint8_t val = 0;
    esp_err_t res = spi_read_reg8(REG_CONFIG, &val);
    if (res != ESP_OK)
        return res;
    val |= BIT_CONFIG_1SHOT;
    val &= ~BIT_CONFIG_AUTO;
    return spi_write_reg(REG_CONFIG, val);
}

static esp_err_t max31865_clear_fault(void)
{
    uint8_t val = 0;
    esp_err_t res = spi_read_reg8(REG_CONFIG, &val);
    if (res != ESP_OK)
        return res;
    val &= ~(BIT_CONFIG_1SHOT | BIT_CONFIG_FAULT_D2 | BIT_CONFIG_FAULT_D3);
    val |= BIT_CONFIG_FAULT_CLEAR;
    return spi_write_reg(REG_CONFIG, val);
}

static float rtd_to_temp(float r_rtd)
{
    const float a = 3.9083e-3f;
    const float b = -5.775e-7f;
    const float r0 = (float)CONFIG_EXAMPLE_RTD_NOMINAL;
    float temp = (sqrtf((a * a - (4 * b)) + (4 * b / r0 * r_rtd)) - a) / (2 * b);
    if (temp >= 0)
        return temp;
    float r = r_rtd / r0 * 100.0f;
    float rpoly = r;
    temp = -242.02f;
    temp += 2.2228f * rpoly;
    rpoly *= r;
    temp += 2.5859e-3f * rpoly;
    rpoly *= r;
    temp -= 4.8260e-6f * rpoly;
    rpoly *= r;
    temp -= 2.8183e-8f * rpoly;
    rpoly *= r;
    temp += 1.5243e-10f * rpoly;
    return temp;
}

void app_main(void)
{
    gpio_config_t io_conf =
    {
        .pin_bit_mask = 1ULL << CONFIG_EXAMPLE_CS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_EXAMPLE_CS_GPIO, 1));
    vTaskDelay(pdMS_TO_TICKS(200));

    spi_bus_config_t bus_cfg =
    {
        .mosi_io_num = CONFIG_EXAMPLE_MOSI_GPIO,
        .miso_io_num = CONFIG_EXAMPLE_MISO_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &bus_cfg, 1));

    spi_device_interface_config_t dev_cfg =
    {
        .clock_speed_hz = MAX31865_MAX_CLOCK_SPEED_HZ,
        .mode = 1,
        .spics_io_num = CONFIG_EXAMPLE_CS_GPIO,
        .queue_size = 1,
        .cs_ena_pretrans = 1
    };
    ESP_ERROR_CHECK(spi_bus_add_device(HOST, &dev_cfg, &spi_dev));

    ESP_LOGI(TAG, "SPI host=%d MOSI=%d MISO=%d SCLK=%d CS=%d", HOST, CONFIG_EXAMPLE_MOSI_GPIO, CONFIG_EXAMPLE_MISO_GPIO, CONFIG_EXAMPLE_SCLK_GPIO, CONFIG_EXAMPLE_CS_GPIO);
    ESP_ERROR_CHECK(max31865_write_config());

    uint8_t cfg = 0;
    if (spi_read_reg8(REG_CONFIG, &cfg) == ESP_OK)
        ESP_LOGI(TAG, "Config readback: 0x%02X", cfg);

    while (1)
    {
        ESP_ERROR_CHECK(max31865_start_one_shot());
        vTaskDelay(pdMS_TO_TICKS(70));

        uint16_t raw = 0;
        esp_err_t res = spi_read_reg16(REG_RTD_MSB, &raw);
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "RTD read failed: %d (%s)", res, esp_err_to_name(res));
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        bool fault = raw & 0x01;
        raw >>= 1;
        float r_rtd = raw * (float)CONFIG_EXAMPLE_RTD_REF / 32768.0f;

        if (fault)
        {
            uint8_t fault_status = 0;
            spi_read_reg8(REG_FAULT_STATUS, &fault_status);
            ESP_LOGE(TAG, "Fault: 0x%02X raw=%u r_rtd=%.6f", fault_status, raw, r_rtd);
            max31865_clear_fault();
            max31865_write_config();
        }
        else
        {
            float temp = rtd_to_temp(r_rtd);
            ESP_LOGI(TAG, "RTD raw=%u r_rtd=%.6f temp=%.4f", raw, r_rtd, temp);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
