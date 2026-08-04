idf_component_register(SRCS "main.c" "display.c" "ui/ui.c"
                    INCLUDE_DIRS "." "ui"
                    REQUIRES max31865 bme280 lvgl esp_lcd
                             nvs_flash esp_wifi esp_netif esp_event mqtt json)
