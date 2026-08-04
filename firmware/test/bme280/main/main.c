#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "bme280.h"

#define I2C_MASTER_SCL_IO 21      /*!< GPIO number for I2C master clock (ESP32-S3, BME280 dedicated bus) */
#define I2C_MASTER_SDA_IO 22      /*!< GPIO number for I2C master data  (ESP32-S3, BME280 dedicated bus) */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */

static const char *TAG = "main";
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t bme280_dev_handle;
static bme280_handle_t bme280 = NULL;

static void i2c_master_init()
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C BUS INSTALL ERROR %d", ret);
        return;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME280_I2C_ADDRESS_DEFAULT,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &bme280_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C ADD DEVICE ERROR %d", ret);
    } else {
        ESP_LOGI(TAG, "I2C bus initialized successfully");
    }
}

void sensor_task(void *arg)
{
    i2c_master_init();

    // vTaskDelay(pdMS_TO_TICKS(1000));

    bme280 = bme280_create(bme280_dev_handle);
    bme280_default_init(bme280);

    // Discard the first reading
    float temp, hum, press;
    bme280_read_temperature(bme280, &temp);
    bme280_read_humidity(bme280, &hum);
    bme280_read_pressure(bme280, &press);


    while (1)
    {
        float temperature = 0.0, humidity = 0.0, pressure = 0.0;
        if (bme280_read_temperature(bme280, &temperature) == ESP_OK &&
            bme280_read_humidity(bme280, &humidity) == ESP_OK &&
            bme280_read_pressure(bme280, &pressure) == ESP_OK)
        {
            ESP_LOGI(TAG, "Temperature: %.2f °C, Humidity: %.2f %%, Pressure: %.2f hPa", temperature, humidity, pressure);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void app_main()
{
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
}
