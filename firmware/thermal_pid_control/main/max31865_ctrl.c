#include <math.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_check.h>
#include <esp_bit_defs.h>
#include <driver/gpio.h>
#include "max31865_ctrl.h"

#define REG_CONFIG 0x00
#define REG_RTD_MSB 0x01
#define REG_FAULT_STATUS 0x07

#define BIT_CONFIG_50HZ BIT(0)
#define BIT_CONFIG_FAULT_CLEAR BIT(1)
#define BIT_CONFIG_FAULT_D2 BIT(2)
#define BIT_CONFIG_FAULT_D3 BIT(3)
#define BIT_CONFIG_3WIRE BIT(4)
#define BIT_CONFIG_1SHOT BIT(5)
#define BIT_CONFIG_AUTO BIT(6)
#define BIT_CONFIG_VBIAS BIT(7)

static const char *TAG = "max31865_ctrl";

static float rtd_to_temp(float r_rtd, float r0)
{
    // First solve the temperature range above 0 degC using the quadratic equation
    const float a = 3.9083e-3f;
    const float b = -5.775e-7f;
    float temp = (sqrtf((a * a - (4.0f * b)) + (4.0f * b / r0 * r_rtd)) - a) / (2.0f * b);
    if (temp >= 0.0f)
    {
        return temp;
    }
    // Use polynomial approximation compensation for the negative temperature range
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

static esp_err_t spi_write_reg(max31865_ctrl_t *dev, uint8_t reg, uint8_t val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = {(uint8_t)(reg | 0x80), val};
    t.tx_buffer = tx;
    t.length = sizeof(tx) * 8;
    return spi_device_transmit(dev->spi_dev, &t);
}

static esp_err_t spi_read_reg8(max31865_ctrl_t *dev, uint8_t reg, uint8_t *val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = {reg, 0x00};
    uint8_t rx[sizeof(tx)];
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    t.length = sizeof(tx) * 8;
    esp_err_t res = spi_device_transmit(dev->spi_dev, &t);
    if (res != ESP_OK)
    {
        return res;
    }
    *val = rx[1];
    return ESP_OK;
}

static esp_err_t spi_read_reg16(max31865_ctrl_t *dev, uint8_t reg, uint16_t *val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    uint8_t tx[] = {reg, 0x00, 0x00};
    uint8_t rx[sizeof(tx)];
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    t.length = sizeof(tx) * 8;
    esp_err_t res = spi_device_transmit(dev->spi_dev, &t);
    if (res != ESP_OK)
    {
        return res;
    }
    *val = ((uint16_t)rx[1] << 8) | rx[2];
    return ESP_OK;
}

static esp_err_t max31865_write_config(max31865_ctrl_t *dev, bool filter_50hz)
{
    // Enable bias voltage, use fixed 3-wire mode, and set the filter frequency according to the configuration
    uint8_t cfg = BIT_CONFIG_VBIAS | BIT_CONFIG_3WIRE;
    if (filter_50hz)
    {
        cfg |= BIT_CONFIG_50HZ;
    }
    return spi_write_reg(dev, REG_CONFIG, cfg);
}

static esp_err_t max31865_start_one_shot(max31865_ctrl_t *dev)
{
    uint8_t cfg = 0;
    ESP_RETURN_ON_ERROR(spi_read_reg8(dev, REG_CONFIG, &cfg), TAG, "read config failed");
    cfg |= BIT_CONFIG_1SHOT;
    cfg &= (uint8_t)(~BIT_CONFIG_AUTO);
    return spi_write_reg(dev, REG_CONFIG, cfg);
}

static esp_err_t max31865_clear_fault(max31865_ctrl_t *dev)
{
    uint8_t cfg = 0;
    ESP_RETURN_ON_ERROR(spi_read_reg8(dev, REG_CONFIG, &cfg), TAG, "read config failed");
    cfg &= (uint8_t)(~(BIT_CONFIG_1SHOT | BIT_CONFIG_FAULT_D2 | BIT_CONFIG_FAULT_D3));
    cfg |= BIT_CONFIG_FAULT_CLEAR;
    return spi_write_reg(dev, REG_CONFIG, cfg);
}

esp_err_t max31865_ctrl_init(max31865_ctrl_t *dev, const max31865_ctrl_config_t *cfg)
{
    if (dev == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // CS is automatically controlled by the hardware SPI; set it high first and wait for the device to stabilize
    gpio_config_t cs_io =
    {
        .pin_bit_mask = 1ULL << cfg->cs_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cs_io), TAG, "cs gpio config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(cfg->cs_gpio, 1), TAG, "cs gpio set failed");
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize the SPI bus
    spi_bus_config_t bus_cfg =
    {
        .mosi_io_num = cfg->mosi_gpio,
        .miso_io_num = cfg->miso_gpio,
        .sclk_io_num = cfg->sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
        .flags = 0
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(cfg->spi_host, &bus_cfg, 1), TAG, "spi bus init failed");

    // Attach the MAX31865 device (SPI Mode 1)
    spi_device_interface_config_t dev_cfg =
    {
        .clock_speed_hz = cfg->spi_clock_hz,
        .mode = 1,
        .spics_io_num = cfg->cs_gpio,
        .queue_size = 1,
        .cs_ena_pretrans = 1
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(cfg->spi_host, &dev_cfg, &dev->spi_dev), TAG, "spi add device failed");

    dev->rtd_ref_ohms = cfg->rtd_ref_ohms;
    dev->rtd_nominal_ohms = cfg->rtd_nominal_ohms;
    dev->filter_50hz = cfg->filter_50hz;
    ESP_RETURN_ON_ERROR(max31865_write_config(dev, dev->filter_50hz), TAG, "max31865 config failed");
    return ESP_OK;
}

esp_err_t max31865_ctrl_read_sample(max31865_ctrl_t *dev, float *rtd_ohms, float *temp_c)
{
    if (dev == NULL || rtd_ohms == NULL || temp_c == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // One-shot conversion mode: trigger one measurement and wait for the conversion to complete
    ESP_RETURN_ON_ERROR(max31865_start_one_shot(dev), TAG, "start one shot failed");
    vTaskDelay(pdMS_TO_TICKS(70));

    uint16_t raw = 0;
    ESP_RETURN_ON_ERROR(spi_read_reg16(dev, REG_RTD_MSB, &raw), TAG, "read raw failed");
    // The least significant bit is the fault flag; shift right by one bit to obtain the valid raw RTD value
    bool fault = (raw & 0x01U) != 0U;
    raw >>= 1;

    *rtd_ohms = (float)raw * dev->rtd_ref_ohms / 32768.0f;
    *temp_c = rtd_to_temp(*rtd_ohms, dev->rtd_nominal_ohms);

    if (!fault)
    {
        return ESP_OK;
    }

    // When a fault occurs, read and clear the fault status to allow recovery in the next cycle
    uint8_t fault_status = 0;
    spi_read_reg8(dev, REG_FAULT_STATUS, &fault_status);
    ESP_LOGE(TAG, "fault=0x%02X raw=%u rtd=%.4f", fault_status, raw, *rtd_ohms);
    max31865_clear_fault(dev);
    max31865_write_config(dev, dev->filter_50hz);
    return ESP_ERR_INVALID_STATE;
}
