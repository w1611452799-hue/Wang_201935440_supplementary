#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Try to initialize the ST7789 display via LVGL.
 * Returns true if the display was successfully initialized,
 * false if no display is connected or init failed.
 * Safe to call even when no display is attached. */
bool display_init(void);

/* Update all display values.  No-ops if the display was not
 * initialized or is absent. */
void display_set_pt100_temperature(float temp);
void display_set_bme280_temperature(float temp);
void display_set_humidity(float humidity);
void display_set_pressure(float pressure);

#ifdef __cplusplus
}
#endif
