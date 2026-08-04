#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_err.h>
#include <driver/spi_master.h>
#include "max31865_ctrl.h"
#include "pwm_ctrl.h"

// SPI controller and clock
#define CTRL_SPI_HOST SPI2_HOST
#define CTRL_SPI_CLK_HZ 1000000

// MAX31865 / RTD parameters
#define CTRL_RTD_NOMINAL_OHMS 100.0f
#define CTRL_RTD_REF_OHMS 400.0f
#define CTRL_FILTER_50HZ 1

// SPI pin definitions
#define CTRL_SPI_MOSI_GPIO 23
#define CTRL_SPI_MISO_GPIO 19
#define CTRL_SPI_SCLK_GPIO 18
#define CTRL_SPI_CS_GPIO 15

// PWM output parameters (MOSFET gate)
#define CTRL_PWM_GPIO 2
#define CTRL_PWM_FREQ_HZ 5000

// Temperature-control target and environmental parameters
#define CTRL_SAMPLE_MS 250
// Target temperature (unit: degC)
#define CTRL_SETPOINT_C 45.0f
// Temperature-difference threshold for switching to PID control (switch when the temperature is within this value below the target; use full-power heating beforehand)
// Allow sufficient margin for thermal inertia: the heating rate is ~1.2C/s and inertial coasting can exceed 20C, so this is set to 20C
#define CTRL_BANG_DELTA_C 20.0f

// Controller parameters: PID
// Kp=2.5: proportional term; Ki=0.03: integral term (reduced to prevent integral accumulation over long periods); Kd=15.0: derivative term (strong braking to suppress thermal inertia in advance)
// Kd/Kp≈9 => when err≈9°C and rate≈1°C/s, duty approaches zero, leaving margin for inertial coasting
#define CTRL_PID_KP 2.0f
#define CTRL_PID_KI 0.03f
#define CTRL_PID_KD 15.0f
// Integral limit (absolute value), preventing residual-heat inertia from driving the temperature beyond the target
#define CTRL_PID_I_LIMIT 3.0f

// Temperature-rate EMA filter factor (alpha): 0.35 = 35% new value + 65% history, reducing amplification of measurement noise by Kd
#define CTRL_RATE_ALPHA 0.35f

// ── HOLD mode parameters ─────────────────────────────────────────────────────
// Temperature-difference threshold for entering HOLD mode (switch to the holding strategy when the temperature is within this value of the target)
#define CTRL_HOLD_ENTRY_MARGIN_C 5.0f
// Temperature-difference threshold for exiting HOLD mode (switch back to heating mode when the temperature falls more than this value below the target)
#define CTRL_HOLD_EXIT_MARGIN_C  10.0f
// HOLD-mode PI parameters (no D term, steady-state holding)
#define CTRL_HOLD_KP         1.0f
// Ki×I_limit must cover the duty required to compensate for steady-state heat loss
// Measured result: maintaining 45°C requires approximately 14-15% duty (ambient temperature approximately 20°C)
// Ki=1.0, I_limit=20 => maximum integral contribution is 20%; steady-state integral≈14.5 (accumulated over approximately 30s)
#define CTRL_HOLD_KI         1.0f
// HOLD uses one-way integration [0, I_limit], allowing rapid decay during overshoot
#define CTRL_HOLD_I_LIMIT    20.0f
// Maximum duty cycle in HOLD mode (limited to prevent excessive output during the holding stage)
#define CTRL_HOLD_MAX_DUTY   25.0f

// Output and safety-protection parameters
// Maximum duty cycle in heating mode (shared by the Bang and PID stages)
// Reducing this to 45% lowers the heater steady-state temperature and reduces stored heat after shutoff, trading a slower heating rate for less overshoot
#define CTRL_MAX_DUTY_PERCENT 45.0f
#define CTRL_SAFETY_MARGIN_C 8.0f
// Safety threshold for the temperature rate (degC/s); exceeding it indicates a possible sensor fault (the Bang stage can reach ~1.5C/s in testing, so 3.0 is used to avoid false detection)
#define CTRL_MAX_HEAT_RATE_CPS 3.0f

static const char *TAG = "thermal_pid";

// Control state machine
typedef enum
{
    CTRL_STATE_HEATING = 0,  // Heating mode: Bang + PID (including the D term), responsible for rapid heating
    CTRL_STATE_HOLD    = 1,  // HOLD mode: conservative PI (without the D term), with limited output for steady-state heat-loss compensation
} ctrl_state_t;

typedef struct
{
    float kp;
    float ki;
    float kd;
    float integral;
    float i_limit;
} pid_ctrl_t;

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

// temp_rate: measured temperature rate (C/s), used as the derivative-term input; positive while the temperature is rising
static float pid_step(pid_ctrl_t *pid, float error, float temp_rate, float dt_s, float out_min, float out_max)
{
    float next_integral = clampf(pid->integral + error * dt_s, -pid->i_limit, pid->i_limit);
    // The D term uses the temperature rate (negative sign: reduce output proactively when the temperature rises quickly)
    float raw = pid->kp * error + pid->ki * next_integral - pid->kd * temp_rate;
    float out = clampf(raw, out_min, out_max);

    if (raw == out)
    {
        pid->integral = next_integral;
    }
    return out;
}

// Dedicated to HOLD mode: one-way integration [0, i_limit], always updated (without anti-windup locking)
// Effect: during overshoot, the integral decreases by |err|*dt_s at each step and decays to zero in approximately 15s, avoiding slow oscillation caused by locking
static float hold_pid_step(pid_ctrl_t *pid, float error, float dt_s, float out_max)
{
    // One-way integration: only positive values are allowed (the heating system cannot actively cool, so a negative integral is meaningless)
    // Always update: allows the integral to decay rapidly to zero during overshoot instead of being locked by anti-windup
    pid->integral = clampf(pid->integral + error * dt_s, 0.0f, pid->i_limit);
    float raw = pid->kp * error + pid->ki * pid->integral;
    return clampf(raw, 0.0f, out_max);
}

static void thermal_control_task(void *arg)
{
    (void)arg;

    // 1) Initialize the temperature-acquisition module (MAX31865)
    max31865_ctrl_t sensor = {0};
    max31865_ctrl_config_t sensor_cfg =
    {
        .spi_host = CTRL_SPI_HOST,
        .spi_clock_hz = CTRL_SPI_CLK_HZ,
        .mosi_gpio = CTRL_SPI_MOSI_GPIO,
        .miso_gpio = CTRL_SPI_MISO_GPIO,
        .sclk_gpio = CTRL_SPI_SCLK_GPIO,
        .cs_gpio = CTRL_SPI_CS_GPIO,
        .rtd_ref_ohms = CTRL_RTD_REF_OHMS,
        .rtd_nominal_ohms = CTRL_RTD_NOMINAL_OHMS,
        .filter_50hz = CTRL_FILTER_50HZ
    };
    ESP_ERROR_CHECK(max31865_ctrl_init(&sensor, &sensor_cfg));

    // 2) Initialize the PWM output module (MOSFET gate control)
    pwm_ctrl_t pwm = {0};
    pwm_ctrl_config_t pwm_cfg =
    {
        .pwm_gpio = CTRL_PWM_GPIO,
        .pwm_freq_hz = CTRL_PWM_FREQ_HZ
    };
    ESP_ERROR_CHECK(pwm_ctrl_init(&pwm, &pwm_cfg));

    // 3) PID parameters
    pid_ctrl_t pid_heat =
    {
        .kp = CTRL_PID_KP,
        .ki = CTRL_PID_KI,
        .kd = CTRL_PID_KD,
        .integral = 0.0f,
        .i_limit = CTRL_PID_I_LIMIT
    };

    // 4) HOLD-mode PI parameters (without the D term)
    pid_ctrl_t pid_hold =
    {
        .kp = CTRL_HOLD_KP,
        .ki = CTRL_HOLD_KI,
        .kd = 0.0f,
        .integral = 0.0f,
        .i_limit = CTRL_HOLD_I_LIMIT
    };

    ctrl_state_t ctrl_state = CTRL_STATE_HEATING;
    bool hold_coast_done = false;  // HOLD-mode inertia-dissipation flag: stop output completely after entering HOLD until the temperature begins to fall

    const float dt_s = (float)CTRL_SAMPLE_MS / 1000.0f;
    const float target_temp = CTRL_SETPOINT_C;

    float prev_temp_c = 0.0f;
    float filtered_rate = 0.0f;  // Temperature rate after EMA filtering
    uint32_t sample_count = 0;

    ESP_LOGI(TAG, "SPI host=%d MOSI=%d MISO=%d SCLK=%d CS=%d",
             CTRL_SPI_HOST, CTRL_SPI_MOSI_GPIO, CTRL_SPI_MISO_GPIO, CTRL_SPI_SCLK_GPIO, CTRL_SPI_CS_GPIO);
    ESP_LOGI(TAG, "PWM gpio=%d freq=%dHz target=%.2fC", CTRL_PWM_GPIO, CTRL_PWM_FREQ_HZ, target_temp);

    while (1)
    {
        // 4) Sampling: read the RTD resistance and temperature
        float r_rtd = 0.0f;
        float temp_c = 0.0f;
        esp_err_t res = max31865_ctrl_read_sample(&sensor, &r_rtd, &temp_c);
        if (res != ESP_OK)
        {
            pwm_ctrl_set_duty_percent(&pwm, 0.0f);
            pid_heat.integral = 0.0f;
            pid_hold.integral = 0.0f;
            ESP_LOGW(TAG, "sensor error, heating off");
            vTaskDelay(pdMS_TO_TICKS(CTRL_SAMPLE_MS));
            continue;
        }

        sample_count++;
        float temp_rate = 0.0f;
        if (sample_count > 1 && dt_s > 0.0f)
        {
            float raw_rate = (temp_c - prev_temp_c) / dt_s;
            // EMA filtering: reduce amplification of measurement noise by the Kd term
            filtered_rate = CTRL_RATE_ALPHA * raw_rate + (1.0f - CTRL_RATE_ALPHA) * filtered_rate;
            temp_rate = filtered_rate;
        }
        prev_temp_c = temp_c;

        // 5) Safety protection: abnormal temperature rate (possible sensor fault)
        if (temp_rate > CTRL_MAX_HEAT_RATE_CPS && temp_c < target_temp - 5.0f)
        {
            pwm_ctrl_set_duty_percent(&pwm, 0.0f);
            pid_heat.integral = 0.0f;
            pid_hold.integral = 0.0f;
            filtered_rate = 0.0f;
            ESP_LOGE(TAG, "temp rate %.2fC/s too high, heating off", temp_rate);
            vTaskDelay(pdMS_TO_TICKS(CTRL_SAMPLE_MS * 4));
            sample_count = 0;
            continue;
        }
        
        // 6) Force heating off and clear the integrals if the safety-temperature threshold is exceeded
        if (temp_c >= target_temp + CTRL_SAFETY_MARGIN_C)
        {
            pwm_ctrl_set_duty_percent(&pwm, 0.0f);
            pid_heat.integral = 0.0f;
            pid_hold.integral = 0.0f;
            filtered_rate = 0.0f;
            ESP_LOGW(TAG, "temp %.2fC exceeds limit, heating off", temp_c);
            vTaskDelay(pdMS_TO_TICKS(CTRL_SAMPLE_MS * 4));
            sample_count = 0;
            continue;
        }
        
        // 7) State-machine transitions
        if (ctrl_state == CTRL_STATE_HEATING && temp_c >= target_temp - CTRL_HOLD_ENTRY_MARGIN_C)
        {
            ctrl_state = CTRL_STATE_HOLD;
            pid_hold.integral = 0.0f;  // Reset the integral when entering HOLD to avoid interference from the heating stage
            hold_coast_done = false;   // Enter the inertia-dissipation period: stop output completely until the temperature begins to fall
            ESP_LOGI(TAG, "-> HOLD mode (temp=%.2fC)", temp_c);
        }
        else if (ctrl_state == CTRL_STATE_HOLD && temp_c < target_temp - CTRL_HOLD_EXIT_MARGIN_C)
        {
            ctrl_state = CTRL_STATE_HEATING;
            pid_heat.integral = 0.0f;  // Reset the integral when returning to heating mode
            ESP_LOGI(TAG, "-> HEATING mode (temp=%.2fC)", temp_c);
        }
        
        // 8) Calculate the output according to the current state
        float duty;
        if (ctrl_state == CTRL_STATE_HEATING)
        {
            // Heating mode: Bang + PID (including the D term)
            if (temp_c < target_temp - CTRL_BANG_DELTA_C)
            {
                // Initial stage: full-power heating
                duty = CTRL_MAX_DUTY_PERCENT;
            }
            else
            {
                // PID closed-loop control (the D term suppresses overshoot caused by thermal inertia)
                float temp_error = target_temp - temp_c;
                duty = pid_step(&pid_heat, temp_error, temp_rate, dt_s, 0.0f, CTRL_MAX_DUTY_PERCENT);
            }
        }
        else
        {
            // HOLD mode
            float temp_error = target_temp - temp_c;
            if (!hold_coast_done)
            {
                // Inertia-dissipation period: stop output completely after entering HOLD until the temperature begins to fall
                // Avoid continued heating during the inertial-rise stage (previously, 10s of unnecessary heat was added in the 42-44°C range)
                duty = 0.0f;
                if (temp_rate < -0.02f)
                {
                    // The temperature has begun to fall, confirming that the inertia has dissipated; start PI control
                    hold_coast_done = true;
                    pid_hold.integral = 0.0f;  // Start integration from zero for a clean start
                    ESP_LOGI(TAG, "coast done (temp=%.2fC rate=%.2fC/s)", temp_c, temp_rate);
                }
            }
            else
            {
                // After inertia dissipation: PI steady-state control (one-way integration, limited to 25%)
                duty = hold_pid_step(&pid_hold, temp_error, dt_s, CTRL_HOLD_MAX_DUTY);
            }
        }

        // 9) Apply the output
        pwm_ctrl_set_duty_percent(&pwm, duty);
        ESP_LOGI(TAG, "[%s] temp=%.2fC rate=%.2fC/s err=%.2f duty=%.1f%%",
                 ctrl_state == CTRL_STATE_HOLD ? "HOLD" : "HEAT",
                 temp_c, temp_rate, target_temp - temp_c, duty);
        vTaskDelay(pdMS_TO_TICKS(CTRL_SAMPLE_MS));
    }
}

void app_main(void)
{
    xTaskCreate(thermal_control_task, "thermal_control_task", 6144, NULL, 5, NULL);
}
