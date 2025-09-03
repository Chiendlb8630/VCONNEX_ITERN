
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

static const char *TAG = "COUNTDOWN";
static countdown_t countdowns[4];  
static esp_timer_handle_t night_timer = NULL;
static esp_timer_handle_t nvs_timer = NULL; 

void start_nvs_timer(void);
int schedule_count = 0;
int mesh_schedule_count = 0;
schedule_item_t schedule_list[MAX_SCHEDULE];

void timer_callback(TimerHandle_t xTimer);
void mesh_timer_callback(TimerHandle_t xTimer);
static void night_timer_callback(void* arg);

static void night_mode_timer_cb(void *arg){
    night_mode_args_t *cfg = (night_mode_args_t *)arg;
    if (cfg) {
        cmd_night_mode_handle(cfg->enb, cfg->begin, cfg->end);
        free(cfg); 
    }
}

void nvs_timer_callback(void *arg) {
    save_states_to_nvs();
}

void shedule_init(){
    if(config_on){
    my_sntp_init();
    xTaskCreatePinnedToCore(obtain_time, "obtain_time", 2048, NULL, 5, NULL, 1);
    } else {
        return;
    }
}

void my_sntp_init(void){
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void obtain_time(void *pvParameter) {
      if (!esp_sntp_enabled()) {
        my_sntp_init();  
    }
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    ESP_LOGI("SNTP", "Time is set!");
    vTaskDelete(NULL); 
}

void Istime(schedule_item_t item){
    time_t now;
    time(&now);
    int64_t delay_sec = (int64_t)item.timestamp - (int64_t)now;
    if (delay_sec <= 0) delay_sec = 1; 
    uint64_t delay_ms = (uint64_t)delay_sec * 1000ULL;
    uint64_t ticks64  = delay_ms / portTICK_PERIOD_MS;
    if (ticks64 == 0) ticks64 = 1;
    if (ticks64 > portMAX_DELAY - 1) ticks64 = portMAX_DELAY - 1;
    TickType_t delay_ticks = (TickType_t)ticks64;
    TimerHandle_t timer = NULL;
    if(config_on){
        timer = xTimerCreate("ScheduleTimer", delay_ticks, pdFALSE, (void*)(intptr_t)item.id, timer_callback);
    } else if(mesh_mode){
        timer = xTimerCreate("Mesh_ScheduleTimer", delay_ticks, pdFALSE, (void*)(intptr_t)item.id, mesh_timer_callback);
        ESP_LOGI("MESH_TIMER","Timer for mesh with id %d, delay=%llds created",
                 item.id, (long long)delay_sec);
    }
    if(timer){
        xTimerStart(timer, 0);
    } else {
        ESP_LOGE("MESH_TIMER","Timer %d create failed", item.id);
    }
}

void timer_callback(TimerHandle_t xTimer) {
    int id = (int)(intptr_t)pvTimerGetTimerID(xTimer);
    printf("Timer event triggered for ID = %d\n", id);
    for (int j = 0; j < schedule_count; j++) {
        if (schedule_list[j].id == id) {
            for (int i = 0; i < 4; i++) {
                if (schedule_list[j].dev_states[i] != 3) {
                    led_states[i] = schedule_list[j].dev_states[i];
                    if(led_states[i]) {
                        led_set_color(led_strips[i], duty, 0, 0);
                    }else {
                        led_set_color(led_strips[i], duty, duty, duty);
                    }
                    if(buzzer_enable) buzzer_beep(200);
                }
            }
            char *msg_timer = build_status_message();
            mqtt_publish_to_app(msg_timer);
            free(msg_timer);
            break; 
        }
    }
    xTimerDelete(xTimer, 0);
}

void mesh_timer_callback(TimerHandle_t xTimer) {
    int id = (int)(intptr_t)pvTimerGetTimerID(xTimer);
    printf("Mesh Timer event triggered for ID = %d\n", id);
    for (int j = 0; j < mesh_schedule_count; j++) {
        printf("%d \n", schedule_list[j].id);
        if (schedule_list[j].id == id) {
            printf("id of schedule %d", id);
            for (int i = 0; i < 4; i++) {
                printf(" : %d \n", schedule_list[j].dev_states[i]);
                if (schedule_list[j].dev_states[i] != 3) {
                    led_states[i] = schedule_list[j].dev_states[i];
                    if (led_states[i]) {
                        led_set_color(led_strips[i], duty, 0, 0);
                    } else {
                        led_set_color(led_strips[i], duty, duty, duty);
                    }
                    buzzer_beep(200);
                }
            }
        }
    }
}

static void countdown_cb(void *arg) {
    countdown_t *cd = (countdown_t *)arg;
    if (cd->remain > 0) {
        cd->remain--;
        ESP_LOGI(TAG, "SW%d remain: %d sec", cd->sw_idx, (int) cd->remain);
    }
    if (cd->remain == 0) {
        esp_timer_stop(cd->h);
        led_set_color(led_strips[(int)cd->sw_idx-1], duty, duty, duty);
        relay_set_state((int)cd->sw_idx - 1, OFF);
        ESP_LOGI(TAG, "SW%d countdown finished -> Light OFF", cd->sw_idx);
    }
}

void app_time_countdown_handle(uint8_t sw_idx, uint32_t cnt_timestamp) {
    if (sw_idx < 1 || sw_idx > 4) return;
    countdown_t *cd = &countdowns[sw_idx - 1];
    cd->sw_idx = sw_idx;
    cd->remain = cnt_timestamp;

    if (cd->h == NULL) {
        esp_timer_create_args_t args = {
            .callback = countdown_cb,
            .arg = cd,
            .name = "cd_timer"
        };
        esp_timer_create(&args, &cd->h);
    } else {
        esp_timer_stop(cd->h); 
    }
    esp_timer_start_periodic(cd->h, 1000000); 
    ESP_LOGI(TAG, "SW%d start countdown %d sec", sw_idx, (int)cnt_timestamp);
}

void cmd_night_mode_handle(uint8_t enb, uint32_t begin, uint32_t end) {
    if (enb == 0) {
<<<<<<< HEAD
        night_timer_callback(NULL);
=======
>>>>>>> b672db7 (first commit)
        printf("Night mode disabled\n");
        return;
    }
    time_t t_begin = (time_t)begin;
    time_t t_end   = (time_t)end;
    ESP_LOGI(TAG, "Raw begin=%ld, end=%ld", (long)t_begin, (long)t_end);
    time_t now;
    time(&now);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    struct tm tm_b, tm_e;
    localtime_r(&t_begin, &tm_b);
    localtime_r(&t_end, &tm_e);
    ESP_LOGI(TAG, "Begin -> %04d-%02d-%02d %02d:%02d:%02d", tm_b.tm_year + 1900, tm_b.tm_mon + 1, tm_b.tm_mday, tm_b.tm_hour, tm_b.tm_min, tm_b.tm_sec);
    ESP_LOGI(TAG, "End   -> %04d-%02d-%02d %02d:%02d:%02d", tm_e.tm_year + 1900, tm_e.tm_mon + 1, tm_e.tm_mday, tm_e.tm_hour, tm_e.tm_min, tm_e.tm_sec);
    int sec_now   = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;
    int sec_begin = tm_b.tm_hour   * 3600 + tm_b.tm_min * 60 + tm_b.tm_sec;
    int sec_end   = tm_e.tm_hour   * 3600 + tm_e.tm_min * 60 + tm_e.tm_sec;
    printf("Now=%d sec, Begin=%d sec, End=%d sec\n", sec_now, sec_begin, sec_end);

    if (sec_end == sec_begin) {
        printf("Invalid night mode time (end == begin)\n");
        return;
    }

    if (sec_end < sec_begin) {
        sec_end += 24 * 3600;
    }

    if (sec_now < sec_begin) {
        int wait = sec_begin - sec_now;
        printf("Waiting %d seconds until night mode starts\n", wait);
        if (night_timer) {
            esp_timer_stop(night_timer);
            esp_timer_delete(night_timer);
            night_timer = NULL;
        }
        night_mode_args_t *cfg = malloc(sizeof(night_mode_args_t));
        cfg->enb = enb;
        cfg->begin = begin;
        cfg->end = end;

        esp_timer_create_args_t args = {
            .callback = &night_timer_callback,
            .arg = cfg,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "night_start_timer"
        };
        esp_timer_create(&args, &night_timer);
        esp_timer_start_once(night_timer, (uint64_t)wait * 1000000ULL);
        return;
    }
    if (sec_now >= sec_begin && sec_now < sec_end) {
        int duration = sec_end - sec_now;
        buzzer_enable = 0;
        led_enable = 0;
        set_led_all_off();
        ex_info.buzzer_enb = 1;
        ex_info.rgbon = 1;
        printf("Night mode started -> OFF for %d seconds\n", duration);
        if (night_timer) {
            esp_timer_stop(night_timer);
            esp_timer_delete(night_timer);
            night_timer = NULL;
        }

        night_mode_args_t *cfg = malloc(sizeof(night_mode_args_t));
        cfg->enb = enb;
        cfg->begin = begin;
        cfg->end = end;

        esp_timer_create_args_t args = {
            .callback = &night_timer_callback,
            .arg = cfg,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "night_end_timer"
        };
        esp_timer_create(&args, &night_timer);
        esp_timer_start_once(night_timer, (uint64_t)duration * 1000000ULL);
        return;
    }
    printf("Night mode window has passed\n");
}

void start_nvs_timer(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &nvs_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nvs_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &nvs_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_timer, 60000000)); 
}

static void night_timer_callback(void* arg) {
    buzzer_enable = 1; 
    ex_info.buzzer_enb = 0;
    led_enable= 1;
    ex_info.rgbon = 1;
    printf("Night mode ended -> buzzer and LED restored\n");
}

void set_time_TAI_to_UNIX(uint8_t *tai_bytes, uint16_t delta){
    uint64_t unix_tsp = 0;
    const uint64_t UNIX_EPOCH_OFFSET_SECONDS = 946684800;
    uint64_t tai_seconds_from_2000 = 0;
    unix_tsp |= (uint64_t)tai_bytes[0] << 32;
    unix_tsp |= (uint64_t)tai_bytes[1] << 24;
    unix_tsp |= (uint64_t)tai_bytes[2] << 16;
    unix_tsp |= (uint64_t)tai_bytes[3] << 8;
    unix_tsp |= (uint64_t)tai_bytes[4]; 

    unix_tsp += UNIX_EPOCH_OFFSET_SECONDS - delta;
    ESP_LOGI(TAG, " timestamp: %lld", unix_tsp);

    struct timeval tv = {
        .tv_sec = (time_t)unix_tsp,
        .tv_usec = 0,
    };
    settimeofday(&tv, NULL);
    gettimeofday(&tv, NULL);
    printf("Seconds: %lld, Microseconds: %lld \n", (long long) tv.tv_sec, (long long)tv.tv_usec);
}








