#include "display.h"
#include "ui.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"

/* ── Pin configuration — defaults match test project ─────────── */
#define LCD_HOST            CONFIG_DC_LCD_SPI_HOST
#define LCD_PIXEL_CLOCK_HZ  (20 * 1000 * 1000)
#define LCD_H_RES           240
#define LCD_V_RES           135
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8

#define PIN_SCLK            CONFIG_DC_LCD_PIN_SCLK
#define PIN_MOSI            CONFIG_DC_LCD_PIN_MOSI
#define PIN_MISO            CONFIG_DC_LCD_PIN_MISO
#define PIN_DC              CONFIG_DC_LCD_PIN_DC
#define PIN_RST             CONFIG_DC_LCD_PIN_RST
#define PIN_CS              CONFIG_DC_LCD_PIN_CS

/* ── LVGL tuning ──────────────────────────────────────────────── */
#define LVGL_DRAW_BUF_LINES     20
#define LVGL_TICK_PERIOD_MS     2
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_MIN_DELAY_MS  (1000 / CONFIG_FREERTOS_HZ)
#define LVGL_TASK_STACK_SIZE    (8 * 1024)
#define LVGL_TASK_PRIORITY      2

static const char *TAG = "display";

static bool            s_ok = false;       /* display initialized okay */
static esp_lcd_panel_io_handle_t   s_io_handle;
static esp_lcd_panel_handle_t      s_panel_handle;
static _lock_t                     s_lvgl_lock;  /* mutex for LVGL API */

/* ── LVGL flush callback ──────────────────────────────────────── */
static bool notify_flush_ready(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    /* ST7789 SPI is big-endian — swap RGB565 byte order */
    lv_draw_sw_rgb565_swap(px_map,
        (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px_map);
}

/* ── LVGL tick ─────────────────────────────────────────────────── */
static void lvgl_tick_cb(void *arg) { lv_tick_inc(LVGL_TICK_PERIOD_MS); }

/* ── LVGL task ─────────────────────────────────────────────────── */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    uint32_t delay;
    while (1) {
        _lock_acquire(&s_lvgl_lock);
        delay = lv_timer_handler();
        _lock_release(&s_lvgl_lock);
        if (delay < LVGL_TASK_MIN_DELAY_MS) delay = LVGL_TASK_MIN_DELAY_MS;
        if (delay > LVGL_TASK_MAX_DELAY_MS) delay = LVGL_TASK_MAX_DELAY_MS;
        usleep(1000 * delay);
    }
}

/* ── Public API ────────────────────────────────────────────────── */

bool display_init(void)
{
    /* ── SPI bus init (separate bus from MAX31865) ───────────── */
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = PIN_SCLK,
        .mosi_io_num     = PIN_MOSI,
        .miso_io_num     = PIN_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    esp_err_t err = spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SPI bus init failed (no display?): %s", esp_err_to_name(err));
        return false;
    }

    /* ── Panel IO ────────────────────────────────────────────── */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num        = PIN_DC,
        .cs_gpio_num        = PIN_CS,
        .pclk_hz            = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits       = LCD_CMD_BITS,
        .lcd_param_bits     = LCD_PARAM_BITS,
        .spi_mode           = 0,
        .trans_queue_depth  = 10,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                   &io_cfg, &s_io_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "panel IO init failed: %s", esp_err_to_name(err));
        return false;
    }

    /* ── ST7789 panel ────────────────────────────────────────── */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &s_panel_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ST7789 panel init failed: %s", esp_err_to_name(err));
        return false;
    }

    /* ── Panel configuration (135x240 landscape) ─────────────── */
    esp_lcd_panel_reset(s_panel_handle);
    err = esp_lcd_panel_init(s_panel_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "panel init sequence failed: %s", esp_err_to_name(err));
        return false;
    }
    esp_lcd_panel_invert_color(s_panel_handle, true);
    esp_lcd_panel_swap_xy(s_panel_handle, true);
    esp_lcd_panel_set_gap(s_panel_handle, 40, 52);
    esp_lcd_panel_mirror(s_panel_handle, false, true);
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    /* ── LVGL init ───────────────────────────────────────────── */
    lv_init();

    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    size_t draw_buf_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buf_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buf_sz, 0);
    if (!buf1 || !buf2) {
        ESP_LOGW(TAG, "DMA buffer alloc failed");
        return false;
    }
    lv_display_set_buffers(disp, buf1, buf2, draw_buf_sz,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, s_panel_handle);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* ── LVGL tick timer ─────────────────────────────────────── */
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb, .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000);

    /* ── Flush ready callback ────────────────────────────────── */
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(s_io_handle, &cbs, disp);

    /* ── LVGL task ───────────────────────────────────────────── */
    xTaskCreate(lvgl_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL,
                LVGL_TASK_PRIORITY, NULL);

    /* ── Build UI ────────────────────────────────────────────── */
    _lock_acquire(&s_lvgl_lock);
    ui_init();
    _lock_release(&s_lvgl_lock);

    s_ok = true;
    ESP_LOGI(TAG, "ST7789 135x240 display ready");
    return true;
}

void display_set_pt100_temperature(float temp)
{
    if (!s_ok) return;
    _lock_acquire(&s_lvgl_lock);
    ui_set_pt100_temperature(temp);
    _lock_release(&s_lvgl_lock);
}

void display_set_bme280_temperature(float temp)
{
    if (!s_ok) return;
    _lock_acquire(&s_lvgl_lock);
    ui_set_bme280_temperature(temp);
    _lock_release(&s_lvgl_lock);
}

void display_set_humidity(float humidity)
{
    if (!s_ok) return;
    _lock_acquire(&s_lvgl_lock);
    ui_set_humidity(humidity);
    _lock_release(&s_lvgl_lock);
}

void display_set_pressure(float pressure)
{
    if (!s_ok) return;
    _lock_acquire(&s_lvgl_lock);
    ui_set_pressure(pressure);
    _lock_release(&s_lvgl_lock);
}
