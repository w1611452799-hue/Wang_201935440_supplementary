#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define TAG                  "wifi_sta"
#define ESP_WIFI_STA_SSID    "your_ssid"
#define ESP_WIFI_STA_PASSWD  "your_password"
#define ESP_WIFI_MAX_RETRY   10

static uint8_t s_connect_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    // WiFi started successfully; initiate the connection
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA started, connecting...");
        esp_wifi_connect();
    }
    // WiFi disconnected or connection failed; retry automatically
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_connect_count < ESP_WIFI_MAX_RETRY) {
            s_connect_count++;
            ESP_LOGW(TAG, "Disconnected, retry %d/%d...", s_connect_count, ESP_WIFI_MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Failed to connect after %d retries, give up.", ESP_WIFI_MAX_RETRY);
        }
    }
    // IP address obtained; connection successful
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_connect_count = 0; // Reset the retry count to support subsequent reconnection after disconnection
        ip_event_got_ip_t *info = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&info->ip_info.ip));
    }
}

void app_main(void)
{
	//----------------Preparation Stage-------------------
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	//----------------Initialization Stage-------------------
	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	// Register the event (WiFi started successfully)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, wifi_event_handler, NULL, NULL));
	// Register the event (WiFi disconnection/reconnection)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_event_handler, NULL, NULL));
	// Register the event (IP address obtained)
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

	// Initialize the STA interface
	esp_netif_create_default_wifi_sta();

	/*Initialize WiFi */
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	// WIFI_INIT_CONFIG_DEFAULT is a macro for the default configuration

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	//----------------Configuration Stage-------------------
	// Initialize the WiFi device (allocate resources for the WiFi driver, such as WiFi control structures, RX/TX buffers, and WiFi NVS structures; this also starts the WiFi task. This API must be called before any other WiFi APIs)
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	// Detailed STA configuration
	wifi_config_t sta_config = {
		.sta = {
			.ssid = ESP_WIFI_STA_SSID,
			.password = ESP_WIFI_STA_PASSWD,
			.bssid_set = false,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

	//----------------Startup Stage-------------------
	ESP_ERROR_CHECK(esp_wifi_start());

	//----------------Power-Saving Mode Configuration-------------------
	// Disable power saving (data transmission will be faster)
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}
