#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "PWM_CONTROL";

// LEDC configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          (2) // Use GPIO 2 for PWM output (change as needed)
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // 13-bit resolution, maximum value 8191
#define LEDC_FREQUENCY          (5000) // 5 kHz frequency

// UART configuration
#define UART_NUM                UART_NUM_0
#define BUF_SIZE                (1024)

/**
 * @brief Initialize the LEDC peripheral
 */
static void ledc_init(void)
{
    // 1. Timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Initial duty cycle is 0
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

/**
 * @brief Initialize UART0
 */
static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    // Install the driver
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
}

/**
 * @brief UART processing task
 * Receive a number (0-100) from UART0 and update the PWM duty cycle
 */
static void uart_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    while (1) {
        // Read data from UART0
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            // Convert the received string to an integer
            int duty_percent = atoi((char *)data);
            
            // Limit the range to 0-100%
            if (duty_percent < 0) duty_percent = 0;
            if (duty_percent > 100) duty_percent = 100;

            // Calculate the actual duty cycle value (based on 13-bit resolution)
            uint32_t duty_val = (duty_percent * 8191) / 100;

            // Update the LEDC duty cycle
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_val));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

            ESP_LOGI(TAG, "接收到占空比: %d%%, 对应数值: %ld", duty_percent, duty_val);
            
            // Echo confirmation
            printf("PWM 占空比已设置为: %d%%\n", duty_percent);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "PWM 控制工程启动...");

    // 1. Initialize LEDC
    ledc_init();

    // 2. Initialize UART0
    uart_init();

    // 3. Create the UART processing task
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
}
