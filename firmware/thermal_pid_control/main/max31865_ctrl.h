#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include <driver/spi_master.h>

typedef struct
{
    spi_host_device_t spi_host;
    int spi_clock_hz;
    int mosi_gpio;
    int miso_gpio;
    int sclk_gpio;
    int cs_gpio;
    float rtd_ref_ohms;
    float rtd_nominal_ohms;
    bool filter_50hz;
} max31865_ctrl_config_t;

typedef struct
{
    spi_device_handle_t spi_dev;
    float rtd_ref_ohms;
    float rtd_nominal_ohms;
    bool filter_50hz;
} max31865_ctrl_t;

esp_err_t max31865_ctrl_init(max31865_ctrl_t *dev, const max31865_ctrl_config_t *cfg);
esp_err_t max31865_ctrl_read_sample(max31865_ctrl_t *dev, float *rtd_ohms, float *temp_c);
