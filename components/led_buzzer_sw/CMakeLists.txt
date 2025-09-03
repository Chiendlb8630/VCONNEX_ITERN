idf_component_register(SRCS "led_buzzer_sw.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__led_strip
                             driver
                             nvs_flash
                             blufi_app
                             app_ble_mesh
                             iic_switch)