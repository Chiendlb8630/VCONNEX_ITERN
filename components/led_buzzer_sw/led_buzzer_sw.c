#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h" 
#include "led_buzzer_sw.h"
#include "driver/ledc.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <inttypes.h>
#include "blufi_app.h"
#include "app_ble_mesh.h"
#include "driver/i2c.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "mqtt_app.h" 
#include "iic_sw.h" 
#include "app_time.h"
#include "app_ota.h"


const char *TAG = "SW_LED_BUZZER";
uint8_t touch_bits[] = { (1<<5), (1<<6), (1<<7) , (1<<2) };
uint8_t prev_touch = 0;
int hold_counters[4];
static int led_gpios[4] = {23,17,27,14};
static int relay_gpios[4] = {26, 25, 16, 4};
int control_mode_msk[4] = {1, 1, 1, 1};
set_color_info_t color_info = {
    .rOn = 255,
    .gOn = 0,
    .bOn = 0,

    .rOff = 255,
    .gOff = 255,
    .bOff = 255
};

int led_count = 4;
led_strip_handle_t led_strips[4];
bool led_states[4] = {false, false, false, false};
int duty = 100;
bool lnIsSet = false;
bool buzzer_enable = 1;
bool led_enable = 1;
bool state_save_on;
bool night_mode = 0;

void run_task(void *pvParameters) {
    ESP_ERROR_CHECK(i2c_init());
    buzzer_init();
    switch_init_led(); 
    uint8_t touch_state = 0;
     if(state_save_on == 1){
        load_state_from_nvs();
        for(int i =0;i < 4;i++){
            printf("%d\n", led_states[i]);
        }
        set_led();
    }
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(deinit_event_group, EVENT_DEINIT_BLE, pdTRUE, pdFALSE, pdMS_TO_TICKS(10));
        if (bits & EVENT_DEINIT_BLE) {
            ESP_LOGI("RUN_TASK", "Deinit BLE Mesh...");
            ble_stack_deinit();
            wifi_config_t wifi_config = {0};
            strncpy((char *)wifi_config.sta.ssid, ble_mesh_wifi.ssid, sizeof(wifi_config.sta.ssid)-1);
            strncpy((char *)wifi_config.sta.password, ble_mesh_wifi.password, sizeof(wifi_config.sta.password)-1);

            ESP_LOGI(TAG, "Connecting to SSID:%s PASS:%s", wifi_config.sta.ssid, wifi_config.sta.password);
            wifi_init_se();
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_start());
            ESP_ERROR_CHECK(esp_wifi_connect());

            xEventGroupWaitBits(wifi_event, CONNECTED_BIT , pdTRUE, pdFALSE, portMAX_DELAY);
            xTaskCreatePinnedToCore(ota_task, "ota_example_task", 8192, NULL, 5, NULL, 1);
        }

        if(!config_on){
        if (Read_touch(&touch_state) == ESP_OK) {
            uint8_t pressed = touch_state & ~prev_touch;
            for (int i = 0; i < led_count; i++) {
                if(touch_state & touch_bits[i]){
                    hold_counters[i] += 10;
                    ESP_LOGI(TAG, "%d ", hold_counters[i]);
                } else {
                    if(MESH_TIME >= hold_counters[i] && hold_counters[i] >= PRESSED_TIME){
                        wifi_pair_mode();
                        hold_counters[i] = 0;
                    }
                    if(hold_counters[i] >= MESH_TIME){
                        ble_mesh_pair_mode();
                        hold_counters[i] = 0;
                    }
                }
                if (pressed & touch_bits[i]) {
                    led_states[i] = !led_states[i];
                    if(mqtt_on){
                        char *msg_pressed = build_status_message();
                        mqtt_publish_to_app(msg_pressed);
                        free(msg_pressed);
                    }
                    if(mesh_mode){
                        BLE_mesh_response_to_gw(i, TOPIC_DEVICE_PUB, (uint8_t)led_states[i]);
                        printf("addr=0x%04" PRIX16 "\n", save_endpoint_ele[i]);
                        if(save_endpoint_ele[i] != 0){
                            send_onoff_set(i, save_endpoint_ele[i], (uint8_t) led_states[i]);
                        }   
                    }

                    if (led_states[i]){
                        if(control_mode_msk[i]){
                        switch_change_state(i ,ON);
                        }
                        print_led_state();   
                    } else {
                        if(control_mode_msk[i]){
                        switch_change_state(i ,OFF);
                        }
                        print_led_state();    
                    }
                    buzzer_beep(300);
                }
            }
            prev_touch = touch_state;
        }
    }else{
        vTaskDelay(pdMS_TO_TICKS(100));
}
}
} 

void wifi_pair_mode(){
    if(mesh_mode){
    ble_stack_deinit();
    wifi_init_se();
    mesh_mode = 0;    
    }
    mesh_flag_save();
    config_on = true;
    blufi_app_init();
}

void ble_mesh_pair_mode(){
    config_on = false;
    esp_wifi_stop();
    esp_wifi_deinit();
    mqtt_stop();
    ble_stack_deinit();
    mesh_mode = 1;
    mesh_flag_save();
    ble_mesh_config();
}

void relay_gpio_init(void) {
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,    
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,  
        .pin_bit_mask = 0                
    };
    for (int i = 0; i < 4; i++) {
        io_conf.pin_bit_mask |= (1ULL << relay_gpios[i]);
    }
    gpio_config(&io_conf);
    for (int i = 0; i < 4; i++) {
        gpio_set_level(relay_gpios[i], 0);
    }
}

void relay_set_state(uint8_t index, bool state) {
    if (index >= (sizeof(relay_gpios)/sizeof(relay_gpios[0]))) {
        return;
    }
    gpio_set_level(relay_gpios[index], state ? 1 : 0);
}

void buzzer_init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = 2731,  
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num       = BUZZER_GPIO,
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .duty           = 0,  
        .hpoint         = 0
    };
    ledc_channel_config(&channel_conf);
}

void set_duty_all_led(){
    for(int i = 0;i< 4;i++){
        if(led_states[i]){
            led_set_color(led_strips[i], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
        } else {
            led_set_color(led_strips[i], (color_info.rOff * duty) / 100, (color_info.gOff * duty) / 100, (color_info.bOff * duty) / 100);        }
    }
}

void buzzer_beep(int duration_ms) {
    if(buzzer_enable){
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 200); 
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void pair_beep(int duration_ms, int pause_ms) {
    for (int i = 0; i < 3; i++) {
        buzzer_beep(duration_ms);       
        vTaskDelay(pdMS_TO_TICKS(pause_ms)); 
    }
}

led_strip_handle_t led_init(int gpio){
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, 
        .resolution_hz = 10 * 1000 *1000,
        .mem_block_symbols = 0, 
        .flags.with_dma = false,
    };

    led_strip_handle_t handle; 
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &handle));
    return handle;
}

void led_set_color(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b) {
    if (led_enable) {
        ESP_LOGI(TAG, "LED ON -> R=%d, G=%d, B=%d", r, g, b);
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, r, g, b));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
    } else {
        ESP_LOGI(TAG, "LED OFF -> R=0, G=0, B=0");
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, 0, 0, 0));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
    }
}

void switch_init_led(void){
    if(!lnIsSet) duty = 255;
    for(int i = 0;i < 4;i++){
        led_strips[i] = led_init(led_gpios[i]);
        if(led_enable ==1){
        led_set_color(led_strips[i], duty, duty, duty);
        } else led_set_color(led_strips[i], 0, 0, 0);
        
    }
}

void print_led_state(){
    for(int i = 0;i<  4;i++){
        printf("%d\n", led_states[i]);
    }
}

void save_states_to_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(nvs_handle, "states", led_states, sizeof(led_states));
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Save states failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_set_u8(nvs_handle, "flag", state_save_on);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Save flag failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    err = nvs_commit(nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI("NVS", "Saved states and flag to NVS");
    } else {
        ESP_LOGE("NVS", "Commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
}

void load_state_from_nvs(void){
    nvs_handle_t nvs_handle;
    size_t required_size = sizeof(led_states);
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if(err != ESP_OK){
        ESP_LOGW("NVS", " No saved states");
        return;
    }
    err = nvs_get_blob(nvs_handle, "states", led_states, &required_size);
    if(err == ESP_OK){
        ESP_LOGW("NVS", "load states");
    } else {
        ESP_LOGW("NVS", "Failed to load %s", esp_err_to_name(err));
    }
}

void load_state(void){
    nvs_handle_t nvs_handle;
    nvs_open("storage", NVS_READONLY, &nvs_handle);
    esp_err_t err = nvs_get_u8(nvs_handle,"flag", &state_save_on);
    if(err == ESP_OK){
        ESP_LOGW("NVS", "load flag done");
    } else {
        ESP_LOGW("NVS", "Failed to load flag : %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
}

void set_led(void){
    for(int i = 0;i <4 ;i++){
        if(led_states[i]){
            led_set_color(led_strips[i], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
        } else {
            led_set_color(led_strips[0], (color_info.rOff * duty)/100, (color_info.gOff * duty) / 100, (color_info.bOff * duty) / 100);
        }
    }
}

void set_led_all_on(void){
    led_set_color(led_strips[0], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
    led_set_color(led_strips[1], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
    led_set_color(led_strips[2], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
    led_set_color(led_strips[3], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
}

void set_led_all_off(void){
    led_set_color(led_strips[0], 0, 0, 0);
    led_set_color(led_strips[1], 0, 0, 0);
    led_set_color(led_strips[2], 0, 0, 0);
    led_set_color(led_strips[3], 0, 0, 0);
}

void switch_change_state(uint8_t idx, uint8_t onoff){
    if(onoff == ON){
        led_set_color(led_strips[idx], (color_info.rOn * duty)/100 , (color_info.gOn * duty) / 100, (color_info.bOn * duty) / 100);
        relay_set_state(idx, ON);
    }
    else {
        led_set_color(led_strips[idx], (color_info.rOff * duty) / 100, (color_info.gOff * duty) / 100, (color_info.bOff * duty) / 100);
        relay_set_state(idx, OFF);
    }
}
