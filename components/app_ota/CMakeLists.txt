idf_component_register(SRCS "app_ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES protocol_examples_common
                    freertos
                    esp_system
                    esp_event
                    esp_http_client
                    esp_https_ota
                    app_update
                    esp_wifi
                    json
                    mqtt_sw
                    blufi_app)