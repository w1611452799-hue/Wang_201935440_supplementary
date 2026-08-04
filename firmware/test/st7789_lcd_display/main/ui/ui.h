#ifndef UI_H
#define UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_set_temperature(float temp);
void ui_set_humidity(float humidity);
void ui_set_pressure(float pressure);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
