#include "ui.h"
#include <stdio.h>

/* ================================================================
 *  135x240 Environment Monitor — 4 sensor cards (no title)
 * ================================================================ */

static lv_obj_t *label_pt100;
static lv_obj_t *label_bme280;
static lv_obj_t *label_humi;
static lv_obj_t *label_pres;

/* Format float to fixed-point string (avoids LVGL printf float issues). */
static void set_label_fixed_1(lv_obj_t *label, float value)
{
    char text[16];
    int scaled = (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    unsigned int magnitude = scaled < 0 ? (unsigned int)(-scaled) : (unsigned int)scaled;

    if (scaled < 0) {
        snprintf(text, sizeof(text), "-%u.%u", magnitude / 10U, magnitude % 10U);
    } else {
        snprintf(text, sizeof(text), "%u.%u", magnitude / 10U, magnitude % 10U);
    }
    lv_label_set_text(label, text);
}

static lv_obj_t *create_card(lv_obj_t *parent, lv_palette_t palette,
                             const char *title, const char *unit)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 224, 28);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_palette_darken(LV_PALETTE_GREY, 4), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(card, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, lv_palette_main(palette), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_size(row, 100, 24);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 4, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, "--.-");
    lv_obj_set_width(val, 64);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, LV_PART_MAIN);

    lv_obj_t *u = lv_label_create(row);
    lv_label_set_text(u, unit);
    lv_obj_set_style_text_color(u, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_text_font(u, &lv_font_montserrat_14, LV_PART_MAIN);

    return val;
}

void ui_init(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 240, 135);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *root = lv_obj_create(scr);
    lv_obj_set_size(root, 240, 135);
    lv_obj_center(root);
    lv_obj_set_style_pad_all(root, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(root, 4, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    label_pt100  = create_card(root, LV_PALETTE_RED,    "PT100 Temp",  "°C");
    label_bme280 = create_card(root, LV_PALETTE_ORANGE, "BME280  Temp", "°C");
    label_humi   = create_card(root, LV_PALETTE_BLUE,   "Humidity",    "%");
    label_pres   = create_card(root, LV_PALETTE_GREEN,  "Pressure",    "hPa");

    lv_scr_load(scr);
}

void ui_set_pt100_temperature(float temp) {
    if (label_pt100) set_label_fixed_1(label_pt100, temp);
}

void ui_set_bme280_temperature(float temp) {
    if (label_bme280) set_label_fixed_1(label_bme280, temp);
}

void ui_set_humidity(float humidity) {
    if (label_humi) set_label_fixed_1(label_humi, humidity);
}

void ui_set_pressure(float pressure) {
    if (label_pres) set_label_fixed_1(label_pres, pressure);
}
