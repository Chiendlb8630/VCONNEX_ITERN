idf_component_register(SRCS  "app_ble_mesh_cmd_handle.c" "app_ble_mesh.c"
                    INCLUDE_DIRS "include"
                    REQUIRES efuse 
                    bt 
                    example_init
                    led_buzzer_sw
                    blufi_app
                    esp_wifi
                    app_time
                    )