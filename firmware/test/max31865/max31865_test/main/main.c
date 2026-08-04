#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <max31865.h>
#include <driver/gpio.h>
#include <esp_idf_lib_helpers.h>

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

#define HOST SPI2_HOST


#define RTD_CONNECTION MAX31865_3WIRE

#if CONFIG_EXAMPLE_SCALE_ITS90
#define RTD_STANDARD MAX31865_ITS90
#endif
#if CONFIG_EXAMPLE_SCALE_DIN43760
#define RTD_STANDARD MAX31865_DIN43760
#endif
#if CONFIG_EXAMPLE_SCALE_US
#define RTD_STANDARD MAX31865_US_INDUSTRIAL
#endif

#if CONFIG_EXAMPLE_FILTER_50
#define FILTER MAX31865_FILTER_50HZ
#endif
#if CONFIG_EXAMPLE_FILTER_60
#define FILTER MAX31865_FILTER_60HZ
#endif

static const char *TAG = "max31865-example";

static max31865_config_t config =
{
    .v_bias = true,
    .filter = FILTER,
    .mode = MAX31865_MODE_SINGLE,
    .connection = RTD_CONNECTION
};

static void task(void *pvParameter)
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

    spi_bus_config_t cfg =
    {
        .mosi_io_num = CONFIG_EXAMPLE_MOSI_GPIO,
        .miso_io_num = CONFIG_EXAMPLE_MISO_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));
    ESP_LOGI(TAG, "SPI host=%d MOSI=%d MISO=%d SCLK=%d CS=%d", HOST, CONFIG_EXAMPLE_MOSI_GPIO, CONFIG_EXAMPLE_MISO_GPIO, CONFIG_EXAMPLE_SCLK_GPIO, CONFIG_EXAMPLE_CS_GPIO);

    // Init device
    max31865_t dev =
    {
        .standard = RTD_STANDARD,
        .r_ref = CONFIG_EXAMPLE_RTD_REF,
        .rtd_nominal = CONFIG_EXAMPLE_RTD_NOMINAL,
    };
    ESP_ERROR_CHECK(max31865_init_desc(&dev, HOST, MAX31865_MAX_CLOCK_SPEED_HZ, CONFIG_EXAMPLE_CS_GPIO));

    // Configure device
    ESP_ERROR_CHECK(max31865_set_config(&dev, &config));
    max31865_config_t read_cfg = { 0 };
    esp_err_t cfg_res = max31865_get_config(&dev, &read_cfg);
    if (cfg_res == ESP_OK)
    {
        ESP_LOGI(TAG, "Config readback: v_bias=%d filter=%d mode=%d connection=%d", read_cfg.v_bias, read_cfg.filter, read_cfg.mode, read_cfg.connection);
    }
    else
        ESP_LOGE(TAG, "Config readback failed: %d (%s)", cfg_res, esp_err_to_name(cfg_res));

    float temperature;
    while (1)
    {
        esp_err_t res = max31865_measure(&dev, &temperature);
        if (res != ESP_OK)
        {
            uint8_t fault_status = 0;
            ESP_LOGE(TAG, "Failed to measure: %d (%s)", res, esp_err_to_name(res));
            if (max31865_get_fault_status(&dev, &fault_status) == ESP_OK)
                ESP_LOGE(TAG, "Fault status: 0x%02X", fault_status);
            uint16_t raw = 0;
            bool fault = false;
            if (max31865_read_raw(&dev, &raw, &fault) == ESP_OK)
            {
                float r_rtd = raw * dev.r_ref / 32768.0f;
                ESP_LOGE(TAG, "RTD raw: %u, fault: %d, r_rtd: %.6f", raw, fault, r_rtd);
            }
            max31865_clear_fault_status(&dev);
            max31865_set_config(&dev, &config);
        }
        else
            ESP_LOGI(TAG, "Temperature: %.4f", temperature);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main()
{
    xTaskCreatePinnedToCore(task, TAG, configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM);
}
