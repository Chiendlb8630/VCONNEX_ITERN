idf_component_register(SRCS "mqtt_app.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi 
                    esp_event 
                    bt nvs_flash 
                    mbedtls 
                    esp_common 
                    json mqtt 
                    protocol_examples_common 
                    espressif__led_strip 
                    blufi_app
                    driver
                    led_buzzer_sw
                    app_time
                    app_ota)