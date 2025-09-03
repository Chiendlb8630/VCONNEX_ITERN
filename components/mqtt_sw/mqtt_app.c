#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "protocol_examples_common.h"
#include "mqtt_app.h"
#include "led_buzzer_sw.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "app_time.h"
#include "nvs.h"
#include "app_ota.h"
#include <inttypes.h>
#include <time.h>

static const char *TAG = "MQTT_APP";
device_config_t mqtt_conf;


#define DEVICE_TYPE       3018
#define DEVICE_EXT_ADDR   "F4CFA24966D0"
#define FIXED_COMMAND     "CmdAddSchedule"
#define FIXED_ID          1
#define FIXED_ERROR_CODE  50000

esp_mqtt_client_config_t mqtt_cfg = {0};
esp_mqtt_client_handle_t client = NULL;
static cJSON *value;

bool mqtt_on = false;

char *fixed_template =
        "{"
        "\"name\":\"CmdSetExtraConfig\","
        "\"devT\":3018,"
        "\"devExtAddr\":\"F4CFA24966D0\","
        "\"buzzerEnb\":1,"
        "\"ledEnb\":1,"
        "\"ledRgbOn\":2507,"
        "\"ledRgbOff\":9630,"
        "\"switch_1_type\":1,"
        "\"switch_1_countdown\":30,"
        "\"switch_1_control_mode\":1,"
        "\"switch_1_led_off\":0,"
        "\"switch_1_rgb_on\":862021,"
        "\"switch_1_rgb_off\":123456,"
        "\"nightModeEnb\":1,"
        "\"nightBegin\":1638928504,"
        "\"nightEnd\":1638928504,"
        "\"nightTz\":7,"
        "\"switch_1_lightness\":100,"
        "\"led_lightness\":100,"
        "\"saveStt\":1"
        "}";

void mqtt_app_start(void);
void mqtt_load_config(void);
void parse_data(const char *data, size_t len);
void mqtt_save_config(const char *broker, const char *user, const char *pass, const char *pub_topic, const char *sub_topic);
void mqtt_publish_to_app(const char *msg);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void handle_mqtt_data(const char *topic, const char *data);

char *build_status_message(void);
char *build_fixed_response(void);
char *build_fixed_extra_config_response(void);
char *build_res_set_schedule(int id, int error_code);
char *build_delete_schedule_response(void);
char *build_device_info_res(void);
void print_schedule_item(schedule_item_t item);


void parse_data(const char *data, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) {
        ESP_LOGE(TAG, "Parse JSON failed");
        return;
    }

    cJSON *value = cJSON_GetObjectItem(root, "value");
    if (value && cJSON_IsObject(value)) {
        cJSON *broker   = cJSON_GetObjectItem(value, "broker");
        cJSON *username = cJSON_GetObjectItem(value, "username");
        cJSON *password = cJSON_GetObjectItem(value, "password");
        cJSON *mqttsub  = cJSON_GetObjectItem(value, "mqttsub");
        cJSON *mqttpub  = cJSON_GetObjectItem(value, "mqttpub");

        if (broker && cJSON_IsString(broker)) 
            strncpy(mqtt_conf.broker, broker->valuestring, sizeof(mqtt_conf.broker)-1);
        if (username && cJSON_IsString(username)) 
            strncpy(mqtt_conf.username, username->valuestring, sizeof(mqtt_conf.username)-1);
        if (password && cJSON_IsString(password)) 
            strncpy(mqtt_conf.password, password->valuestring, sizeof(mqtt_conf.password)-1);
        if (mqttsub && cJSON_IsString(mqttsub)) 
            strncpy(mqtt_conf.mqttsub, mqttsub->valuestring, sizeof(mqtt_conf.mqttsub)-1);
        if (mqttpub && cJSON_IsString(mqttpub)){
            strncpy(mqtt_conf.mqttpub, mqttpub->valuestring, sizeof(mqtt_conf.mqttpub)-1);
        }
            mqtt_save_config( mqtt_conf.broker, mqtt_conf.username, mqtt_conf.password, mqtt_conf.mqttpub, mqtt_conf.mqttsub);
    } else {
        ESP_LOGE(TAG, "Missing or invalid 'value' object");
    }

    ESP_LOGI(TAG, "Broker: %s",   mqtt_conf.broker);
    ESP_LOGI(TAG, "Username: %s", mqtt_conf.username);
    ESP_LOGI(TAG, "Password: %s", mqtt_conf.password);
    ESP_LOGI(TAG, "MQTT Sub: %s", mqtt_conf.mqttsub);
    ESP_LOGI(TAG, "MQTT Pub: %s", mqtt_conf.mqttpub);

    cJSON_Delete(root);
}

void mqtt_app_start(void){
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = mqtt_conf.broker,
             .verification = {
            .certificate = NULL, 
            },
        },
        .credentials = {
            .username = mqtt_conf.username,
            .authentication.password = mqtt_conf.password,
        },
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, mqtt_conf.mqttsub, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        mqtt_on = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_on = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
         ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        char *topic = strndup(event->topic, event->topic_len);
        char *payload = strndup(event->data, event->data_len);

        ESP_LOGI(TAG, "TOPIC=%s", topic);
        ESP_LOGI(TAG, "DATA=%s", payload);
        
        handle_mqtt_data(topic, payload);
        free(topic);
        free(payload);
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

int get_channel_from_param(const char *param) {
    if (strncmp(param, "switch_", 7) == 0) {
        const char *arg = param + 7;
        return atoi(arg);  
    }
    else if(strncmp(param, "all", 3) == 0){
        return 100;
    }
    return -1; 
}

void handle_mqtt_data(const char *topic, const char *data) {
    ESP_LOGI(TAG, "Received topic: %s", topic);
    ESP_LOGI(TAG, "Received payload: %s", data);

    cJSON *root = cJSON_Parse(data);
    if (!root) {
        ESP_LOGE(TAG, "JSON Parse Error");
        return;
    }
    const cJSON *name = cJSON_GetObjectItem(root, "name");
    value = cJSON_GetObjectItem(root, "value");
    if (!cJSON_IsString(name) || !cJSON_IsObject(value)) {
        ESP_LOGE(TAG, "Invalid JSON structure");
        cJSON_Delete(root);
        return;
    }
    if (strcmp(name->valuestring, "CmdSetData") == 0) {
        char *msg = build_status_message();
        esp_mqtt_client_publish(client, mqtt_conf.mqttpub , msg, 0, 1, 0);

        const cJSON *devV = cJSON_GetObjectItem(value, "devV");
        if (cJSON_IsObject(devV)) {
            const cJSON *param = cJSON_GetObjectItem(devV, "param");
            const cJSON *val   = cJSON_GetObjectItem(devV, "value");

            if (cJSON_IsString(param) && cJSON_IsNumber(val)) {
                int channel = get_channel_from_param(param->valuestring) - 1;
                int state   = val->valueint;

                if (channel == 99) {
                    for (int i = 0; i < 4; i++) {
                        led_states[i] = (state == 1);
                        if (state == 1){
                            if(control_mode_msk[i]){
                            led_set_color(led_strips[i], 255, 0, 0);  
                            relay_set_state(i, ON);
                            }
                        }        
                        else {
                            if(control_mode_msk[i]){
                            led_set_color(led_strips[i], 255, 255, 255);  
                            relay_set_state(i, 0);
                            }
                        }
                    buzzer_beep(200); 
                }

                ESP_LOGI(TAG, "Channel: %d, State: %d", channel + 1, state);

                if (channel >= 0 && channel <= 3 && led_strips[channel] != NULL) {
                    led_states[channel] = (state == 1);
                    if (state == 1){
                        if(control_mode_msk[channel]){
                        led_set_color(led_strips[channel], 255, 0, 0);
                        relay_set_state(channel, ON );
                        buzzer_beep(300);
                    }
                }  
                    else {
                        if(control_mode_msk[channel]){
                        led_set_color(led_strips[channel], 255, 0, 0);
                        relay_set_state(channel, OFF );
                        buzzer_beep(300);
                    }
                    } 
                } else {
                    ESP_LOGE(TAG, "Invalid LED channel or not initialized: %d", channel + 1);
                }
            }
        }
        msg = build_status_message();
        esp_mqtt_client_publish(client, mqtt_conf.mqttpub , msg, 0, 1, 0);
        free(msg);
    }
}

    else if (strcmp(name->valuestring, "CmdGetData") == 0) {
        char *msg = build_status_message();
        esp_mqtt_client_publish(client, mqtt_conf.mqttpub , msg, 0, 1, 0);
        free(msg);
    }

    else if(strcmp(name->valuestring, "CmdSetExtraConfig")== 0){
        const cJSON *buzzerEnb = cJSON_GetObjectItem(value, "buzzerEnb");
        const cJSON *ledEnb    = cJSON_GetObjectItem(value, "ledEnb");
        const cJSON *ledRgbOn  = cJSON_GetObjectItem(value, "ledRgbOn");
        const cJSON *ledRgbOff = cJSON_GetObjectItem(value, "ledRgbOff");
        const cJSON *swType    = cJSON_GetObjectItem(value, "switch_1_type");
        const cJSON *swCd      = cJSON_GetObjectItem(value, "switch_1_countdown");
        const cJSON *saveStt   = cJSON_GetObjectItem(value, "saveStt");

        if(cJSON_IsNumber(buzzerEnb)){
            buzzer_enable = buzzerEnb ->valueint;
            ESP_LOGI(TAG, "buzzer is set to : %d\n", buzzer_enable);
        }

        if(cJSON_IsNumber(ledEnb)){
            led_enable = ledEnb ->valueint;
            if(led_enable == 0){
            for(int i = 0;i< 4;i++) led_set_color(led_strips[i], 0, 0, 0);
            }
            else {
            for(int i = 0;i< 4;i++) led_set_color(led_strips[i], 255, 255, 255);
            ESP_LOGI(TAG, "Led is set to : %d\n", led_enable);
            }
        }
        if(cJSON_IsNumber(saveStt)){
            state_save_on = saveStt -> valueint;
            ESP_LOGI(TAG, "Save state is set to : %d\n", state_save_on);
                save_states_to_nvs();
        }
        char *res = build_fixed_extra_config_response();
        mqtt_publish_to_app(res);
        free(res);
    }

    else if(strcmp(name->valuestring, "CmdGetExtraConfig")== 0){
        char *res = build_fixed_extra_config_response();
        mqtt_publish_to_app(res);
        free(res);
    }

    else if (strcmp(name->valuestring, "CmdAddSchedule") == 0) {
    schedule_count++;
    cJSON *id = cJSON_GetObjectItem(value, "id");
    if (id && cJSON_IsNumber(id)) {
        schedule_list[schedule_count-1].id = id->valueint;
    }

    cJSON *condition = cJSON_GetObjectItem(value, "condition");
    if (condition) {
        cJSON *values = cJSON_GetObjectItem(condition, "values");
        if (cJSON_IsArray(values) && cJSON_GetArraySize(values) > 0) {
            cJSON *cond_val = cJSON_GetArrayItem(values, 0);
            if (cond_val) {
                cJSON *loop = cJSON_GetObjectItem(cond_val, "loopDays");
                cJSON *exec = cJSON_GetObjectItem(cond_val, "executionTime");

                if (loop && cJSON_IsNumber(loop)) {
                    schedule_list[schedule_count-1].loop = loop->valueint;
                }
                if (exec && cJSON_IsNumber(exec)) {
                    schedule_list[schedule_count-1].timestamp = (uint64_t)exec->valuedouble;
                    }
                }
            }
        }
    
    for(int i = 0;i<4;i++){
        schedule_list[schedule_count-1].dev_states[i] = 3;
    }
    cJSON *action_array = cJSON_GetObjectItem(value, "action");
    if (cJSON_IsArray(action_array) && cJSON_GetArraySize(action_array) > 0) {
        cJSON *action = cJSON_GetArrayItem(action_array, 0);
        if (action) {
            cJSON *devV = cJSON_GetObjectItem(action, "devV");
            if (cJSON_IsArray(devV)) {
                int devV_size = cJSON_GetArraySize(devV);
                for (int i = 0; i < devV_size; i++) {
                    cJSON *dev = cJSON_GetArrayItem(devV, i);
                    if (!dev) continue;

                    cJSON *param_item = cJSON_GetObjectItem(dev, "param");
                    cJSON *value_item = cJSON_GetObjectItem(dev, "value");
                    if (!param_item || !cJSON_IsString(param_item) || !value_item || !cJSON_IsNumber(value_item)) continue;

                    const char *param = param_item->valuestring;
                    int value = value_item->valueint;

                    if (strncmp(param, "switch_", 7) == 0) {
                        int idx = atoi(param + 7) - 1;
                        if (idx >= 0 && idx < 4) {
                            schedule_list[schedule_count-1].dev_states[idx] = (value != 0);
                        }
                    }
                }
            }
        }
    }
    Istime(schedule_list[schedule_count-1]);
    ESP_LOGI(TAG, "Schedule ID: %d | Timestamp: %" PRId64 " | Loop: %d", schedule_list[schedule_count-1].id, schedule_list[schedule_count-1].timestamp, schedule_list[schedule_count-1].loop);
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "Switch_%d: %s", i + 1, schedule_list[schedule_count-1].dev_states[i] ? "ON" : "OFF");
    }
    print_schedule_item(schedule_list[schedule_count-1]);
    char *msg = build_res_set_schedule(schedule_list[schedule_count-1].id , 50000);
    mqtt_publish_to_app(msg);
    free(msg);
    } 
    
    else if(strcmp(name->valuestring, "CmdDeleteSchedule")== 0){
        char *msg = build_delete_schedule_response();
        mqtt_publish_to_app(msg);
        free(msg);
        schedule_count = 0;
    }

    else if(strcmp(name->valuestring, "CmdGetDeviceInfo") == 0){
        char *msg = build_device_info_res();
        mqtt_publish_to_app(msg);
        free(msg);
    }

    else if(strcmp(name->valuestring, "CmdStartOta") == 0 ){
        handle_ota_data(value);
        ota_IsRunning = true;
        xTaskCreatePinnedToCore(ota_task, "ota_example_task", 8192, NULL, 5, NULL, 1);
    }
    else {
        ESP_LOGW(TAG, "Unknown command: %s", name->valuestring);
    }

    cJSON_Delete(root);
}

void mqtt_publish_to_app(const char *msg) {
    int msg_id = esp_mqtt_client_publish(client,mqtt_conf.mqttpub,msg,0,1,0);
    ESP_LOGI("MQTT", "Publish message: %s", msg);    
    if (msg_id == -1) {
        ESP_LOGE("MQTT", "Pub mqtt failed");
    } else {
        ESP_LOGI("MQTT", "Published , msg_id=%d", msg_id);
    }
}

void mqtt_save_config(const char *broker, const char *username, const char *pass, const char *pub_topic, const char *sub_topic){
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return;
    nvs_set_str(nvs, "broker", broker);
    nvs_set_str(nvs, "username", username);
    nvs_set_str(nvs, "mqtt_pass", pass);
    nvs_set_str(nvs, "mqtt_pub", pub_topic);
    nvs_set_str(nvs, "mqtt_sub", sub_topic);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void mqtt_load_config(){
    nvs_handle_t nvs;
    size_t size;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if(err != ESP_OK) return;

     size = sizeof(mqtt_conf.broker);
    nvs_get_str(nvs, "broker", mqtt_conf.broker, &size);

    size = sizeof(mqtt_conf.username);
    nvs_get_str(nvs, "username", mqtt_conf.username, &size);

    size = sizeof(mqtt_conf.password);
    nvs_get_str(nvs, "mqtt_pass", mqtt_conf.password, &size);

    size = sizeof(mqtt_conf.mqttpub);
    nvs_get_str(nvs, "mqtt_pub", mqtt_conf.mqttpub, &size);

    size = sizeof(mqtt_conf.mqttsub);
    nvs_get_str(nvs, "mqtt_sub", mqtt_conf.mqttsub, &size);

    nvs_close(nvs);
}

char *build_fixed_extra_config_response(void) {
    cJSON *response = cJSON_Parse(fixed_template);
    if (!response) {
        ESP_LOGE(TAG, "Failed to parse default template");
        return NULL;
    }
    cJSON_ReplaceItemInObject(response, "buzzerEnb", cJSON_CreateNumber(buzzer_enable));
    cJSON_ReplaceItemInObject(response, "ledEnb", cJSON_CreateNumber(led_enable));
    cJSON_ReplaceItemInObject(response, "saveStt", cJSON_CreateNumber(state_save_on));

    char *out1 = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    return out1;  
}

char * build_res_set_schedule(int id, int error_code){
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "name", "CmdAddSchedule");
    cJSON_AddNumberToObject(root, "id", id);
    cJSON_AddNumberToObject(root, "devT", 3018);
    cJSON_AddStringToObject(root, "devExtAddr", "F4CFA24966D0");
    cJSON_AddNumberToObject(root, "timestamp", 123456789);
    cJSON_AddNumberToObject(root, "errorCode",error_code);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);  
    return json_str;     
}

char* build_delete_schedule_response(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "name", "CmdDeleteSchedule");
    cJSON_AddNumberToObject(root, "devT", 3018);
    cJSON_AddStringToObject(root, "devExtAddr", "F4CFA24966D0");
    cJSON_AddNumberToObject(root, "timestamp", 123456789);
    cJSON_AddNumberToObject(root, "id", -1);
    cJSON_AddNumberToObject(root, "errorCode", 50000);

    char *json_str = cJSON_PrintUnformatted(root);  
    cJSON_Delete(root);
    return json_str;  
}

void print_schedule_item(schedule_item_t item) {
    printf("ID: %u\n", item.id);
    // printf("Time: %02u:%02u\n", item.hour, item.minute);
    printf("Loop Days: 0x%02X\n", item.loop);

    printf("Device states: ");
    for (int i = 0; i < 4; i++) {
        printf("%d ", item.dev_states[i]);
    }
    printf("\n");
}

char *build_status_message(void) {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", "CmdGetData");
    cJSON_AddNumberToObject(root, "devT", 3018);
    cJSON_AddNumberToObject(root, "batteryPercent", 100);
    cJSON_AddStringToObject(root, "devExtAddr", "F4CFA24966D0");
    cJSON_AddNumberToObject(root, "timeStamp", 1576030582);

    cJSON *devV_array = cJSON_CreateArray();
    for (int i = 0; i < 4; i++) {
        cJSON *item = cJSON_CreateObject();
        char param_name[16];
        snprintf(param_name, sizeof(param_name), "switch_%d", i + 1);
        cJSON_AddStringToObject(item, "param", param_name);
        cJSON_AddNumberToObject(item, "value", led_states[i] ? 1 : 0);
        cJSON_AddItemToArray(devV_array, item);
    }
    cJSON_AddItemToObject(root, "devV", devV_array);

    char *json_str = cJSON_PrintUnformatted(root);  
    cJSON_Delete(root);  
    return json_str;  
}

char * build_device_info_res(){
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", "CmdGetDeviceInfo");
    cJSON_AddNumberToObject(root, "devT", 3018);
    cJSON_AddStringToObject(root, "devExtAddr", "F4CFA24966D0");
    cJSON_AddNumberToObject(root, "timeStamp", 1576030582);

    cJSON *devV = cJSON_CreateArray();
    cJSON *devV_item = cJSON_CreateObject();
    cJSON_AddStringToObject(devV_item, "param", "software_version");
    cJSON_AddStringToObject(devV_item, "value", "1.1");

    cJSON_AddItemToArray(devV, devV_item);

    cJSON_AddItemToObject(root, "devV", devV);

    char *json_str = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "JSON Response: %s", json_str);

    cJSON_Delete(root);
    return json_str;
}

void mqtt_stop(void)
{
    if (client) {
        esp_mqtt_client_stop(client);   // Dừng loop MQTT
        esp_mqtt_client_destroy(client); // Giải phóng bộ nhớ & handle
        client = NULL;
    }
}