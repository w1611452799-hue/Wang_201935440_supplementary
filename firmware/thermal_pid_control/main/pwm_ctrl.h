#pragma once

#include <esp_err.h>

typedef struct
{
    int pwm_gpio;
    int pwm_freq_hz;
} pwm_ctrl_config_t;

typedef struct
{
    int reserved;
} pwm_ctrl_t;

esp_err_t pwm_ctrl_init(pwm_ctrl_t *ctrl, const pwm_ctrl_config_t *cfg);
esp_err_t pwm_ctrl_set_duty_percent(pwm_ctrl_t *ctrl, float duty_percent);
