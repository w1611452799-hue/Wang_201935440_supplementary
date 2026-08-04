#include <esp_check.h>
#include <driver/ledc.h>
#include "pwm_ctrl.h"

#define CTRL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define CTRL_LEDC_TIMER LEDC_TIMER_0
#define CTRL_LEDC_CHANNEL LEDC_CHANNEL_0
#define CTRL_LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define CTRL_LEDC_DUTY_MAX ((1U << 13) - 1U)

static float clampf(float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

esp_err_t pwm_ctrl_init(pwm_ctrl_t *ctrl, const pwm_ctrl_config_t *cfg)
{
    if (ctrl == NULL || cfg == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Configure the LEDC timer: determines the PWM frequency and resolution
    ledc_timer_config_t timer_cfg =
    {
        .speed_mode = CTRL_LEDC_MODE,
        .timer_num = CTRL_LEDC_TIMER,
        .duty_resolution = CTRL_LEDC_DUTY_RES,
        .freq_hz = cfg->pwm_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), "pwm_ctrl", "ledc timer config failed");

    // Configure the LEDC channel: bind it to a specific GPIO with an initial duty cycle of 0
    ledc_channel_config_t channel_cfg =
    {
        .speed_mode = CTRL_LEDC_MODE,
        .channel = CTRL_LEDC_CHANNEL,
        .timer_sel = CTRL_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = cfg->pwm_gpio,
        .duty = 0,
        .hpoint = 0
    };
    return ledc_channel_config(&channel_cfg);
}

esp_err_t pwm_ctrl_set_duty_percent(pwm_ctrl_t *ctrl, float duty_percent)
{
    if (ctrl == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Map the percentage duty cycle to the raw LEDC count value
    float limited = clampf(duty_percent, 0.0f, 100.0f);
    uint32_t duty = (uint32_t)((limited / 100.0f) * (float)CTRL_LEDC_DUTY_MAX);
    ESP_RETURN_ON_ERROR(ledc_set_duty(CTRL_LEDC_MODE, CTRL_LEDC_CHANNEL, duty), "pwm_ctrl", "ledc set duty failed");
    return ledc_update_duty(CTRL_LEDC_MODE, CTRL_LEDC_CHANNEL);
}
