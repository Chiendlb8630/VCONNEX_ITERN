#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include "esp_efuse.h"  
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_bt_main.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_time_scene_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "led_buzzer_sw.h"
#include "app_ble_mesh.h"
#include "ble_mesh_example_init.h"
#include "blufi_app.h"
#include "app_ota.h"
#include "esp_wifi.h"
#include "app_time.h"

#define TAG "APP_BLE_MESH"

void build_res_cmd_getDevice();
void cmd_set_endpoint_handle(const uint8_t *info, size_t info_len);
void save_endpoint_config(uint8_t index, uint16_t endpoint_ele);
void cmd_delete_endpoint_handle(const uint8_t *msg, size_t len);
esp_err_t endpoint_link_save(void);
esp_err_t endpoint_link_load(void);
void cmd_add_schedule(uint8_t *data, size_t len, schedule_item_t *item);
void buid_res_add_schedule(uint8_t *msg, size_t len, uint8_t err_code);
void cmd_set_wifi_handle(const uint8_t *info, size_t info_len);
void start_ota_handle(const uint8_t *info , size_t info_len);
void cmd_control_mode_handle(uint8_t * msg, size_t len);

void add_arr_type_msg(uint8_t *msg, uint8_t ParamId, uint8_t len, uint8_t *arr);	
void add_str_type_msg(uint8_t *msg, uint8_t ParamId , uint8_t len, char *str);    
void add_value_type_msg(uint8_t *msg, uint8_t ParamId,  uint8_t type, uint64_t value);

void cmd_get_extra_config_handle(uint8_t* cmd, size_t len);
void cmd_night_mode_parse_data(uint8_t* cmd, size_t len);    

void build_res_get_extra_config();
void cmd_extra_set_sw_type(uint8_t* cmd);

void build_extra_res_sucess_cod(uint8_t *model_mes, uint8_t len, uint8_t error_code);
void reset_pointer();
void delay_ms(int ms);

void build_res_cmd_getDevice(){
    add_value_type_msg(msg, 0x01, U16, DEVT);
    add_arr_type_msg(msg, 0x02, sizeof(pu8Mac), pu8Mac);
    add_str_type_msg(msg, 0x03, 3, "1.1");
}

void cmd_set_endpoint_handle(const uint8_t *info, size_t info_len){
    size_t index = 0;
    uint16_t endpoint_ele = 0;
    uint16_t element_index = 0;
        uint8_t id =  info[index++];
        uint8_t len = info[index++];    
       
        if(id >= 0x04 && id <= 0x07){
            switch(id){
                case 0x04:
                    element_index = 0;
                break;

                case 0x05:
                    element_index = 1;
                break;

                case 0x06:
                    element_index = 2;
                break;
                case 0x07:
                    element_index = 3;
                break;
                default: 
                break;
            }
            endpoint_ele = (info[index] << 8) | info[index+1]; 
            ESP_LOGI(TAG, "Endpoint element Id is 0x%04X" , endpoint_ele);
        }
        else {
            ESP_LOGI(TAG, "Unhandle ParamId");
        }
        save_endpoint_config(element_index, endpoint_ele);
        ESP_ERROR_CHECK(endpoint_link_save());
}
void save_endpoint_config(uint8_t index, uint16_t endpoint_ele){
    save_endpoint_ele[index] = endpoint_ele;
}

void cmd_delete_endpoint_handle(const uint8_t *msg, size_t len)
{
    if (!msg || len < 3) {
        ESP_LOGE(TAG, "invalid frame (len=%u)", (unsigned)len);
        return;
    }
    uint8_t x = msg[2];
    if (x == 0x08) {
        memset(save_endpoint_ele, 0, sizeof(save_endpoint_ele));
        if (endpoint_link_save() == ESP_OK) {
            ESP_LOGI(TAG, "Deleted ALL endpoints -> saved to NVS");
        } else {
            ESP_LOGE(TAG, "Failed to save NVS after delete-all");
        }
        return;
    }
    if (x >= 0x01 && x <= 0x04) {
        uint8_t idx = (uint8_t)(x - 1);    
        uint16_t old = save_endpoint_ele[idx];
        save_endpoint_ele[idx] = 0;
        if (endpoint_link_save() == ESP_OK) {
            ESP_LOGI(TAG, "Deleted endpoint at channel %u (idx=%u), old=0x%04X",
                     (unsigned)x, (unsigned)idx, (unsigned)old);
        } else {
            ESP_LOGE(TAG, "Failed to save NVS after delete ch%u", (unsigned)x);
        }
        return;
    }
    ESP_LOGW(TAG, "unknown delete code 0x%02X (ignore)", x);
}
esp_err_t endpoint_link_save(void) {
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("endpoint", NVS_READWRITE, &h));
    esp_err_t err = nvs_set_blob(h, "ele_map", save_endpoint_ele, sizeof(save_endpoint_ele));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
esp_err_t endpoint_link_load(void){
    nvs_handle_t h; size_t sz = sizeof(save_endpoint_ele);
    ESP_ERROR_CHECK(nvs_open("endpoint", NVS_READWRITE, &h));
    esp_err_t err = nvs_get_blob(h, "ele_map", save_endpoint_ele, &sz);
    if (err == ESP_ERR_NVS_NOT_FOUND) { 
        err = ESP_OK;
    }
    nvs_close(h);
    return err;
}

void buid_res_add_schedule(uint8_t *msg, size_t len, uint8_t err_code) {
    msgPointer = len;
    msg[msgPointer] = err_code; 
    msgPointer++;        
}

void cmd_set_wifi_handle(const uint8_t *info, size_t info_len)
{
    size_t index = 0;
    while (index + 2 <= info_len) {
        uint8_t id = info[index++];
        uint8_t len = info[index++];
        if (index + len > info_len) break; 
        if (id == 0x03 && len <= 32) { 
            memcpy(ble_mesh_wifi.ssid, &info[index], len);
            ble_mesh_wifi.ssid[len] = '\0';
            ESP_LOGI(TAG, "Saved SSID: %s", ble_mesh_wifi.ssid);
        } else if (id == 0x04 && len <= 64) { 
            memcpy(ble_mesh_wifi.password, &info[index], len);
            ble_mesh_wifi.password[len] = '\0';
            ESP_LOGI(TAG, "Saved PASS: %s", ble_mesh_wifi.password);
        }
        index += len;
    }
    ble_mesh_wifi.valid = true; 
}
void start_ota_handle(const uint8_t *info , size_t info_len){
    size_t index = 0;
    while( index + 2 <= info_len){
        uint8_t id = info[index++];
        uint8_t len = info[index++];    
        if(index + len >info_len) break;
        if (id == 0x03) {
            strncpy(ble_mesh_wifi.url, (char *)&info[index], len);
            ble_mesh_wifi.url[len] = '\0';   
            ESP_LOGI(TAG, "OTA URL is : %s", ble_mesh_wifi.url);
        }
        else if(id == 0x04 ){
            memcpy(ble_mesh_wifi.firm, &info[index], len);
            ble_mesh_wifi.firm[len] = '\0';
            ESP_LOGI( TAG, "Version : %s", ble_mesh_wifi.firm);
        }
        index += len;
    }
    handle_ota_data_mesh((const char *)ble_mesh_wifi.url);
    xEventGroupSetBits(deinit_event_group, EVENT_DEINIT_BLE);
    ota_IsRunning = true;
}

void cmd_control_mode_handle(uint8_t * msg, size_t len){
    size_t i = 0;
    while (i < len) {
        uint8_t tag = msg[i++];
        uint8_t L   = msg[i++];
        uint8_t *V  = &msg[i];

        if (tag >= CMD_EXCONFIG_SW1_CTRL_MODE && tag <= CMD_EXCONFIG_SW4_CTRL_MODE) {
            int idx = tag - CMD_EXCONFIG_SW1_CTRL_MODE; 
            printf("SW%d control_mode = %u\n", idx+1, V[0]);
            if (V[0] == 0x03) {
                control_mode_msk[idx] = 0;
                ex_info.sw_ctrl_mode[idx] = 0;
            }
            else if(V[0] == 0x01){
                control_mode_msk[idx] = 1; 
                ex_info.sw_ctrl_mode[idx] = 1;
            }
        } 
        else if (tag >= 0x13 && tag <= 0x16) { 
            int idx = tag - 0x13; 
            printf("SW%d led_off = %s\n", idx+1, V[0] ? "true" : "false");
            if (V[0] == 0) {
                led_set_color(led_strips[idx], 0, 0, 0);
            } else {
                led_set_color(led_strips[idx], duty, duty, duty);
            }
        }
        i += L; 
    }
}

void add_arr_type_msg(uint8_t *msg, uint8_t ParamId, uint8_t len, uint8_t *arr){
	*(msg +msgPointer++) = ParamId;
	*(msg +msgPointer++) = len;
	for (int i=0;i<len;i++){
		*(msg +msgPointer++) = *(arr+i);
	}
}
void add_str_type_msg(uint8_t *msg, uint8_t ParamId , uint8_t len, char *str){
    *(msg+ msgPointer++) = ParamId;
    *(msg+ msgPointer++) = len;
    for(int i = 0;i<len;i++){
        *(msg+msgPointer++) = (uint8_t)*(str +i);
    }
}
void add_value_type_msg(uint8_t *msg, uint8_t ParamId,  uint8_t type, uint64_t value){

    *(msg + msgPointer++) = ParamId;
    uint8_t len = 0;
    switch(type){
        case U16:
            len = 2;
        break;
        case U8:
            len = 1;
        break;
        case I32:
            len = 4;
        break;
        case By:
            len = 1;
        break;
        case Bo:
            len = 1;
        break;
        default: 
        break;
    }
    *(msg + msgPointer++) = len;
    for(int i = len - 1; i>=0;i--){
        *(msg + msgPointer++) = (value >>(i*8)) &0xFF;
    }
}

void build_res_get_extra_config(){
    printf("--------------------------------get extra config response---------------------------------\n");
    add_value_type_msg(msg, CMD_EXCONFIG_BUZ_ENB,Bo,ex_info.buzzer_enb);
    add_value_type_msg(msg, CMD_EXCONFIG_LED_ENB,Bo,ex_info.rgbon);
    add_value_type_msg(msg, CMD_EXCONFIG_LIGHTNESS, U8 ,ex_info.liNess);
    add_value_type_msg(msg, CMD_EXCONFIG_SAVE_STATE, Bo ,ex_info.save_status);
    for(int i = 0; i< 4; i++){
        add_value_type_msg(msg, CMD_EXCONFIG_SW1_CTRL_MODE + i, U8, 1- ex_info.sw_ctrl_mode[i]);
    }
    for(int i = 0; i< 4;i++){
        add_value_type_msg(msg, CMD_EXCONFIG_SW1_LED_OFF + i, U8, 1- ex_info.sw_led_off[i]);
    }
    for(int i = 0; i<msgPointer; i++){
        printf(" %02X", msg[i]);
    }
    printf("\n");

}

void build_extra_res_sucess_cod(uint8_t *model_mes, uint8_t len, uint8_t error_code){
    memcpy(msg, model_mes, len);
    msgPointer = len;
    msg[msgPointer] = error_code;
    ESP_LOG_BUFFER_HEX(TAG, msg, msgPointer+1);
    Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG_STT, msg , len +1);
    reset_pointer();
}

void reset_pointer(){
    msgPointer = 0;
    ESP_LOGI(POINTER_TAG," Pointer is reset to 0x%02X", msgPointer);
}

void delay_ms(int ms)
{
	vTaskDelay(ms/portTICK_PERIOD_MS);
}

void build_extra_ctr_res(uint8_t cmd){
    uint8_t i = cmd - 0xF; 
    add_value_type_msg(msg, cmd, U8, 1- ex_info.sw_ctrl_mode[i]);
    add_value_type_msg(msg, cmd + 4, U8, 1- ex_info.sw_led_off[i]);
    
    printf("------------------get extra control mode payload----------------------\n");
    for (int j = 0; j < msgPointer; j++) {
    printf("%02X ", msg[j]);
    }   
    printf("\n");
}

void cmd_get_extra_night_mode_handle(uint8_t* cmd, size_t len){
    uint8_t i = 0;
    add_value_type_msg(msg, CMD_EXCONFIG_NIGHT_ENB, U8, ex_info.night_enb);
    uint8_t buf[4];
    uint32_t val = ex_info.night_begin;
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8)  & 0xFF;
    buf[3] =  val        & 0xFF;
    add_arr_type_msg(msg, CMD_EXCONFIG_NIGHT_BEGIN, 4, buf);
    val = ex_info.night_end;
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8)  & 0xFF;
    buf[3] =  val        & 0xFF;
    add_arr_type_msg(msg, CMD_EXCONFIG_NIGHT_END, 4, buf);
    add_value_type_msg(msg, CMD_EXCONFIG_NIGHT_TZ, U8, 0x07);

    ESP_LOGI(TAG, "------------------------------------Final TLV msg (night mode):-----------------------------");
    ESP_LOG_BUFFER_HEX(TAG, msg, msgPointer);

    Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG_STT, msg, msgPointer);
    reset_pointer();
}

void cmd_get_extra_countdown_handle(uint8_t* cmd, size_t len){

}

void cmd_get_extra_config_handle(uint8_t* cmd, size_t len){
        uint8_t tag = cmd[0];
        if (tag >= CMD_EXCONFIG_SW1_CTRL_MODE && tag <= CMD_EXCONFIG_SW4_CTRL_MODE) {
            build_extra_ctr_res(tag);
            Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG_STT, msg, msgPointer);
            reset_pointer();
        } 
        else if (tag == 0x33) {
            build_res_get_extra_config();
            Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG_STT, msg, msgPointer);
            reset_pointer();
        }
        else if(tag >= CMD_EXCONFIG_NIGHT_ENB && tag <= CMD_EXCONFIG_NIGHT_END){
            cmd_get_extra_night_mode_handle(cmd, len);
        }
        else if(tag >= CMD_EXCONFIG_SW1_TYPE && tag <= CMD_EXCONFIG_SW4_TYPE){
            cmd_get_extra_countdown_handle(cmd, len);
        }
}

void cmd_night_mode_parse_data(uint8_t* cmd, size_t len){
    size_t i = 0;
    while (i < len){
        if (i + 1 >= len) break; 
        uint8_t tag = cmd[i++];
        uint8_t L   = cmd[i++];
        switch(tag){
            case CMD_EXCONFIG_NIGHT_ENB: 
                if (L == 1 && i + 1 <= len){
                    ex_info.night_enb = cmd[i];
                }
                break;

            case CMD_EXCONFIG_NIGHT_BEGIN: 
                if (L == 4 && i + 4 <= len){
                    ex_info.night_begin = ((uint32_t)cmd[i]   << 24) | ((uint32_t)cmd[i+1] << 16) |  ((uint32_t)cmd[i+2] << 8)  |  ((uint32_t)cmd[i+3]);
                }
                break;
            case CMD_EXCONFIG_NIGHT_END: 
                if (L == 4 && i + 4 <= len){
                    ex_info.night_end = ((uint32_t)cmd[i]   << 24) | ((uint32_t)cmd[i+1] << 16) | ((uint32_t)cmd[i+2] << 8) | ((uint32_t)cmd[i+3]);
                }
                break;

            case 0x26:
                if (L == 1 && i + 1 <= len){
                    printf("nightTz = %u\n", cmd[i]);
                }
                break;

            default:
                printf("Unknown tag: 0x%02X\n", tag);
                break;
        }
        i += L; 
    }
    cmd_night_mode_handle(ex_info.night_enb, ex_info.night_begin, ex_info.night_end);
}

void cmd_extra_set_sw_type(uint8_t* cmd){
    uint8_t channel_param = cmd[0];
    uint8_t idx = channel_param - CMD_EXCONFIG_SW1_TYPE;
    if(cmd[2] == 0x01){
        ex_info.sw_type[idx] = 0;
    }
    if(cmd[2] == 0x02)  {
        ex_info.sw_type[idx] = 1;
        app_time_countdown_handle(idx+1, ex_info.sw_countdown[idx]);
    }    
}

void cmd_set_color_on_handle(uint8_t* cmd){
    color_info.rOn = cmd[3];
    color_info.gOn = cmd[4]; 
    color_info.bOn = cmd[5];
    set_duty_all_led();
}    

void cmd_set_color_off_handle(uint8_t* cmd){
    color_info.rOff = cmd[3];
    color_info.gOff = cmd[4]; 
    color_info.bOff = cmd[5];
    set_duty_all_led();
}
