#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>   
#include <stdbool.h>  
#include "led_buzzer_sw.h"
#include "app_time.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include "freertos/timers.h"
#include "mqtt_app.h"
#include "app_ble_mesh.h"
#include "blufi_app.h"
#include "esp_timer.h"

#define MAX_SCHEDULE 10
#define TIMESERVER_TAI_TIME_START 946684768

typedef struct 
{
    int id;
    uint64_t timestamp;
    int loop;   
    int dev_states[4];
}schedule_item_t;

typedef struct {
    uint8_t sw_idx;         
    uint32_t remain;        
    esp_timer_handle_t h;   
} countdown_t;

typedef struct {
    uint8_t  enb;
    uint32_t begin;
    uint32_t end;
} night_mode_args_t;

extern schedule_item_t schedule_list[MAX_SCHEDULE];
extern int schedule_count;
extern int mesh_schedule_count;

void my_sntp_init(void);
void obtain_time(void *pvParameter);
void cmd_night_mode_handle(uint8_t enb, uint32_t begin, uint32_t end);
void Istime();
void app_time_countdown_handle(uint8_t index, uint32_t cnt_timestamp);
void set_time_TAI_to_UNIX(uint8_t *tai_bytes, uint16_t delta);
void start_nvs_timer(void);





