idf_component_register(SRCS "app_time.c"
                    INCLUDE_DIRS "include"
                    REQUIRES led_buzzer_sw
                            mqtt_sw
                            app_ble_mesh
                            )