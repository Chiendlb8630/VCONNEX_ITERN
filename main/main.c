#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "led_strip.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "blufi_app.h"
#include "mqtt_app.h" 
#include "iic_sw.h" 
#include "nvs_flash.h"
#include "led_buzzer_sw.h" 
#include "app_time.h"
#include "app_ble_mesh.h"
#include "app_ota.h"

static const char *TAG = "APP_MAIN";

void app_main(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    relay_gpio_init();
    load_state();
    if(state_save_on){
        start_nvs_timer();
    }
    ESP_ERROR_CHECK(endpoint_link_load());
    mesh_load_config();
    wifi_event = xEventGroupCreate();
    deinit_event_group = xEventGroupCreate();
    wifi_init();
    esp_wifi_set_ps(WIFI_PS_NONE);
    setenv("TZ", "UTC-7", 1);
    tzset();
    printf(" Save value set to %d", state_save_on);
    if(mesh_mode){
        esp_wifi_stop();
        esp_wifi_deinit();
        mqtt_stop();
        printf("Mesh mode value : %" PRIu8 "\n", mesh_mode);
        ble_mesh_config();
    }
    if(state_save_on){
        load_state_from_nvs();
        for(int i =0;i < 4;i++){
            printf("%d\n", led_states[i]);
        }
    }
    xTaskCreatePinnedToCore(run_task, "run_task", 4096, NULL, 2, NULL, 1);
}




