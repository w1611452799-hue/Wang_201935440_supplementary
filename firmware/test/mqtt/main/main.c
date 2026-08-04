#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "mqtt_client.h"
#include "cJSON.h"

/* -------- WiFi Configuration -------- */
#define WIFI_SSID           "your_ssid"
#define WIFI_PASSWD         "your_password"
#define WIFI_MAX_RETRY      5

/* -------- MQTT Configuration -------- */
/* Use the EMQX global public test broker (MQTTS, port 8883) */
#define MQTT_BROKER_URI     "mqtts://broker.emqx.io:8883"
#define MQTT_CLIENT_ID      "esp32_plant_water_" CONFIG_IDF_TARGET
/* Dedicated topics: include a device identifier to avoid conflicts with public broadcast topics */
#define MQTT_PUB_TOPIC      "fr_plant_water/esp32/sensors"
#define MQTT_SUB_TOPIC      "fr_plant_water/esp32/cmd"

static const char *TAG = "mqtt_test";

/* Embedded certificate (broker.emqx.io-ca.crt) */
extern const uint8_t broker_emqx_io_ca_crt_start[] asm("_binary_broker_emqx_io_ca_crt_start");
extern const uint8_t broker_emqx_io_ca_crt_end[]   asm("_binary_broker_emqx_io_ca_crt_end");

/* MQTT client handle (global) */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* WiFi retry count */
static uint8_t s_wifi_retry = 0;

/* Generate a random floating-point number */
static float random_float(float min, float max)
{
    return min + (esp_random() % 1000) / 1000.0f * (max - min);
}

/* Build and publish sensor JSON data */
static void publish_sensor_data(float temp, float humi, float pres)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddNumberToObject(root, "temperature", roundf(temp * 100) / 100.0f);
    cJSON_AddNumberToObject(root, "humidity",    roundf(humi * 100) / 100.0f);
    cJSON_AddNumberToObject(root, "pressure",    roundf(pres * 100) / 100.0f);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ESP_LOGI(TAG, "Publish -> %s : %s", MQTT_PUB_TOPIC, json_str);
        esp_mqtt_client_publish(s_mqtt_client, MQTT_PUB_TOPIC, json_str, 0, 1, 0);
        free(json_str);
    }
    cJSON_Delete(root);
}

/* MQTT event handling */
static void mqtt_event_handler(void *args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event  = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
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
        ESP_LOGI(TAG, "Publish ack, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Received [%.*s]: %.*s",
                 event->topic_len, event->topic,
                 event->data_len,  event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error: type=%d", event->error_handle->error_type);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "esp-tls err=0x%x, tls_stack_err=0x%x, sock_errno=%d",
                     event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_tls_stack_err,
                     event->error_handle->esp_transport_sock_errno);
        }
        break;
    default:
        break;
    }
}

/* Start the MQTT client */
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri              = MQTT_BROKER_URI,
        .broker.verification.certificate = (const char *)broker_emqx_io_ca_crt_start,
        .credentials.client_id           = MQTT_CLIENT_ID,
    };
    s_mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

/* WiFi event callback */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifi_retry < WIFI_MAX_RETRY) {
            s_wifi_retry++;
            ESP_LOGW(TAG, "WiFi disconnected, retry %d/%d", s_wifi_retry, WIFI_MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "WiFi connect failed after %d retries", WIFI_MAX_RETRY);
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retry = 0;
        ip_event_got_ip_t *info = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&info->ip_info.ip));
        mqtt_app_start();
    }
}

/* WiFi STA initialization */
static void wifi_sta_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START,       wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,         wifi_event_handler, NULL, NULL));

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

void app_main(void)
{
    /* NVS initialization */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Start WiFi (MQTT starts automatically after a successful connection) */
    wifi_sta_init();

    /* Continuously collect and report simulated data (once every 10s) */
    vTaskDelay(pdMS_TO_TICKS(3000)); /* Wait for the WiFi/MQTT connection */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        float temp = random_float(10.0f, 35.0f);    /* Temperature: 10~35 °C */
        float humi = random_float(30.0f, 90.0f);    /* Humidity: 30~90% */
        float pres = random_float(950.0f, 1050.0f); /* Pressure: 950~1050 hPa */

        ESP_LOGI(TAG, "Simulated: temp=%.2f humi=%.2f pres=%.2f", temp, humi, pres);

        if (s_mqtt_client) {
            publish_sensor_data(temp, humi, pres);
        }
    }
}

