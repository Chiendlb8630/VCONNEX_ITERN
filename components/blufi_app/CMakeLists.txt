idf_component_register(SRCS "blufi_app.c" "blufi_security.c" "blufi_init.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi 
                    esp_event 
                    bt nvs_flash 
                    mbedtls 
                    esp_common 
                    json mqtt 
                    protocol_examples_common 
                    espressif__led_strip 
                    driver
                    mqtt_sw)