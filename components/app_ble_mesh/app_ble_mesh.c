
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


uint8_t dev_uuid[16] = {0xdd, 0xdd};
uint8_t pu8Mac[6] = {0}; 
uint16_t save_endpoint_ele[4] = {0};
uint16_t uintCurrentVer[3] = {2,0,0};
uint8_t msg[250];
uint8_t mesh_mode = 0;
uint8_t msgPointer = 0;
extraConf_info ex_info = {0};

EventGroupHandle_t deinit_event_group;
mesh_info_t mesh_key;
struct timeval tv;

wifi_ble_mesh_ota ble_mesh_wifi = {0};

uint8_t* app_wifi_get_mac_uint();
uint8_t* app_ble_mesh_get_uuid();
esp_err_t appBleMesh_bindAppKey();
void BLE_mesh_response_to_gw(uint8_t element,uint16_t add, uint8_t value);
void Res_vendor_modal(uint16_t add,uint32_t opcode, uint8_t *data, uint8_t len);
void res_ok_vendor(uint16_t add, uint32_t opcode);
void mesh_send_status();
void save_endpoint_config(uint8_t index, uint16_t endpoint_ele);

esp_err_t endpoint_link_save(void); 
esp_err_t endpoint_link_load(void);

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param);
static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param);
static void ble_mesh_generic_modal_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param);
static void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param);

static esp_err_t ble_mesh_init(void);
void delay_ms(int ms);
void ble_mesh_config(void);
void ble_stack_deinit(void);
void dele_mesh_old_config(void);
void cmd_add_schedule(uint8_t *data, size_t len, schedule_item_t *item);

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
     .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 10),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(1, 10),
};

esp_ble_mesh_time_state_t time_server_state = {
    .time_role = ESP_BLE_MESH_TIME_RELAY,
};
static esp_ble_mesh_time_setup_srv_t time_setup_server = {
    .rsp_ctrl.status_auto_rsp = 0,
    .state = &time_server_state,
};

static esp_ble_mesh_time_srv_t bleMesh_sTimeserver = {
    .state = &time_server_state,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(bleMesh_sTimeserverPub, 1, ROLE_NODE);

static esp_ble_mesh_client_t onoff_client1;
static esp_ble_mesh_client_t onoff_client2;
static esp_ble_mesh_client_t onoff_client3;
static esp_ble_mesh_client_t onoff_client4;

ESP_BLE_MESH_MODEL_PUB_DEFINE(pub_goos_1, 1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(pub_goos_2, 1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(pub_goos_3, 1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(pub_goos_4, 1, ROLE_NODE);

// onoff client 
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub1, 2+1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub2, 2+1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub3, 2+1, ROLE_NODE);
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_cli_pub4, 2+1, ROLE_NODE);

static esp_ble_mesh_gen_onoff_srv_t onoff_server_data_0 = {
		.rsp_ctrl.status_auto_rsp = 0,
};
static esp_ble_mesh_gen_onoff_srv_t onoff_server_data_1 = {
		.rsp_ctrl.status_auto_rsp = 0,
};
static esp_ble_mesh_gen_onoff_srv_t onoff_server_data_2 = {
		.rsp_ctrl.status_auto_rsp = 0,
};

static esp_ble_mesh_gen_onoff_srv_t onoff_server_data_3 = {
		.rsp_ctrl.status_auto_rsp = 0,
};

static esp_ble_mesh_model_op_t vnd_op_control[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SET_DATA, 1),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_GET_DATA, 1),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_op_t vnd_op_config[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_GETDEVICEINFO , 1),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG, 1),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SETWIFIINFO	  , 1), 
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_STARTOTA	  , 1),
    
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_GETENDPOINTCONFIG, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SETENDPOINTCONFIG, 1),
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_DELETEENDPOINTCONFIG, 1),

    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEADD, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEDELETE, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SCHEDULELIST, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEGET, 1),
	ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEUPDATE, 1),
    ESP_BLE_MESH_MODEL_OP_END
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_TIME_SETUP_SRV(&time_setup_server),
    ESP_BLE_MESH_MODEL_TIME_SRV(&bleMesh_sTimeserverPub, &bleMesh_sTimeserver),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&pub_goos_1, &onoff_server_data_0),
	ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub1, &onoff_client1),
};

esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER_CONFIG,
    vnd_op_config, NULL, NULL),
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_SERVER_CONTROL,
    vnd_op_control, NULL, NULL),
};

static esp_ble_mesh_model_t extend_model_1[] = {
	ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&pub_goos_2, &onoff_server_data_1),
	ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub2, &onoff_client2),
};

static esp_ble_mesh_model_t extend_model_2[] = {
	ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&pub_goos_3, &onoff_server_data_2),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub3, &onoff_client3),
};

static esp_ble_mesh_model_t extend_model_3[] = {
	ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&pub_goos_4, &onoff_server_data_3),
	ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_cli_pub4, &onoff_client4),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
	ESP_BLE_MESH_ELEMENT(0, extend_model_1, ESP_BLE_MESH_MODEL_NONE),
	ESP_BLE_MESH_ELEMENT(0, extend_model_2, ESP_BLE_MESH_MODEL_NONE),
	ESP_BLE_MESH_ELEMENT(0, extend_model_3, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
};

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx 0x%03x, addr 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags 0x%02x, iv_index 0x%08" PRIx32, flags, iv_index);
}

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
            param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
            param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
            param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_RESET_EVT");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            mesh_key.net_key = param->value.state_change.appkey_add.net_idx;
            mesh_key.app_key = param->value.state_change.appkey_add.app_idx;
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                param->value.state_change.appkey_add.net_idx,
                param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            {
            uint16_t element_addr = esp_ble_mesh_get_primary_element_address();
            uint16_t app_idx = param->value.state_change.appkey_add.app_idx;
            uint16_t company_id = CID_ESP; 
            uint16_t model_id = ESP_BLE_MESH_VND_MODEL_ID_SERVER_CONTROL; 
            delay_ms(500);
            
            uint16_t element_count = esp_ble_mesh_get_element_count();
            ESP_LOGI(TAG, "Binding AppKey 0x%04X to all models in %d elements", app_idx, element_count);

           esp_err_t err =  appBleMesh_bindAppKey();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to bind AppKey to model (err %d)", err);
            } else {
                ESP_LOGI(TAG, "AppKey bound to model successfully");
            }
            }

            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            break;
        default:
            break;
        }
    }
}

static void ble_mesh_generic_modal_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param) {
       printf("[GEN_SRV] Event: %d, From: 0x%04X -> To: 0x%04X, OpCode: 0x%08" PRIx32,
       event,
       param->ctx.addr,
       param->ctx.recv_dst,
       param->ctx.recv_op);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
      if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            ESP_LOGI(TAG, "onoff 0x%02x, tid 0x%02x", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            uint16_t channel_index = param->ctx.recv_dst -  esp_ble_mesh_get_primary_element_address();
            led_states[channel_index] = (bool)param->value.set.onoff.onoff;
            if(led_states[channel_index]){
                if(control_mode_msk[channel_index]){
                    switch_change_state(channel_index ,ON);
                    send_onoff_set(channel_index, save_endpoint_ele[channel_index], 1);
                    buzzer_beep(300);
                }
            } else {
                if(control_mode_msk[channel_index]){
                switch_change_state(channel_index ,OFF);
                send_onoff_set(channel_index, save_endpoint_ele[channel_index], 0);
                buzzer_beep(300);
                }
            }
            BLE_mesh_response_to_gw(channel_index, TOPIC_DEVICE_PUB, param->value.set.onoff.onoff);
        }
        break;
    default:
        printf("Unhandled event\n");
        break;
    }
}

void send_onoff_set(uint8_t element, uint16_t dst_elem_addr, uint8_t onoff)
{
    ESP_LOGI(TAG, "0x%08X", element);
    esp_ble_mesh_client_common_param_t common = {0};
    common.opcode        = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK; 
    common.model         = onoff_client1.model;            
    common.ctx.net_idx   = mesh_key.net_key;              
    common.ctx.app_idx   = mesh_key.app_key;              
    common.ctx.addr      = dst_elem_addr;        
    common.ctx.send_ttl  = 3;     
    
    if (element==1)      common.model = onoff_client1.model;
	else if (element==2) common.model = onoff_client2.model;
	else if (element==3) common.model = onoff_client3.model;
	else if (element==4) common.model = onoff_client4.model;

   esp_ble_mesh_generic_client_set_state_t set_state = {
		.onoff_set.op_en = false,
		.onoff_set.onoff = onoff,
		.onoff_set.tid = 0,
	};     

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OnOff Set send failed 0x%X", err);
    } else {
        ESP_LOGI(TAG, "OnOff Set src elem 0x%04X -> dst 0x%04X value %u",
                 element, dst_elem_addr, onoff);
    }
}

static void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT: {
    if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SET_DATA){
        ESP_LOGI(TAG,"-----------------------------------------------------Set data command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        uint8_t control_byte = param->model_operation.msg[2];
        ESP_LOGI(TAG, "Control byte for all channel is 0x%02X:", control_byte);
        if(control_byte){
            set_led_all_on(); buzzer_beep(300);
        } else {
            set_led_all_off(); buzzer_beep(300);
        }
        BLE_mesh_response_to_gw(4,TOPIC_DEVICE_PUB, control_byte);
    }

    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG){
        ESP_LOGI(TAG,"-----------------------------------------------------Set extra config command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        uint8_t cmd = param->model_operation.msg[0];
        switch (cmd) {
            case CMD_EXCONFIG_BUZ_ENB :
                uint8_t buzzer_enb_byte = param->model_operation.msg[2];
                ex_info.buzzer_enb = buzzer_enb_byte;
                if(buzzer_enb_byte){
                    buzzer_enable = true;
                } else {
                    buzzer_enable = false;
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
            break;
            case CMD_EXCONFIG_LED_ENB: 
                uint8_t led_enb_byte = param->model_operation.msg[2];
                ex_info.rgbon = led_enb_byte;
                if(led_enb_byte) {
                    led_enable = 1;
                    set_led();
                }
                else {
                    led_enable = 0;
                    set_led_all_off();
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            case CMD_EXCONFIG_SAVE_STATE : 
                uint8_t save_stt = param->model_operation.msg[2];
                ex_info.save_status = save_stt;
                if(save_stt) {
                    state_save_on = 1;
                    save_states_to_nvs();
                }
                else state_save_on = 0;
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);    
            break;
            case CMD_EXCONFIG_LIGHTNESS:
                duty = (int) param->model_operation.msg[2];
                ESP_LOGI("LIGHTNESS",": %d \n", duty);
                ex_info.liNess = duty;
                lnIsSet = true;
                set_duty_all_led();
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length , 0x00);
                break;
            case CMD_EXCONFIG_RGBON : {
                cmd_set_color_on_handle(param->model_operation.msg);
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length , 0x00);
                break;
            } 
            case CMD_EXCONFIG_RGBOFF : {
                cmd_set_color_off_handle(param->model_operation.msg);
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length , 0x00);
                break;
            } 
            case  CMD_EXCONFIG_SW1_TYPE :
            case  CMD_EXCONFIG_SW2_TYPE :
            case  CMD_EXCONFIG_SW3_TYPE :
            case  CMD_EXCONFIG_SW4_TYPE : {
                cmd_extra_set_sw_type(param->model_operation.msg);
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            }
            case CMD_EXCONFIG_SW1_COUNTDOWN: {
                uint8_t * data = param->model_operation.msg;
                uint32_t cnt_timestamp = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16)|((uint32_t)data[4] << 8) |((uint32_t)data[5]);
                ESP_LOGI("CONTDOWN_SW4", "%ld ", (long)cnt_timestamp);
                ex_info.sw_countdown[0] = cnt_timestamp;
                if(ex_info.sw_type[0]){
                app_time_countdown_handle(1,ex_info.sw_countdown[0]);
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            }   
            case CMD_EXCONFIG_SW2_COUNTDOWN: {
                uint8_t * data = param->model_operation.msg;
                uint32_t cnt_timestamp = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16)|((uint32_t)data[4] << 8) |((uint32_t)data[5]);
                ESP_LOGI("CONTDOWN_SW4", "%ld ", (long)cnt_timestamp);
                ex_info.sw_countdown[1] = cnt_timestamp;
                if(ex_info.sw_type[1]){
                app_time_countdown_handle(2,ex_info.sw_countdown[1]);
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            }
            case CMD_EXCONFIG_SW3_COUNTDOWN: {
                uint8_t * data = param->model_operation.msg;
                uint32_t cnt_timestamp = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16)|((uint32_t)data[4] << 8) |((uint32_t)data[5]);
                ESP_LOGI("CONTDOWN_SW4", "%ld ", (long)cnt_timestamp);
                ex_info.sw_countdown[2] = cnt_timestamp;
                if(ex_info.sw_type[2]){
                app_time_countdown_handle(3,ex_info.sw_countdown[2]);
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            }
            case CMD_EXCONFIG_SW4_COUNTDOWN: {
                uint8_t * data = param->model_operation.msg;
                uint32_t cnt_timestamp = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16)|((uint32_t)data[4] << 8) |((uint32_t)data[5]);
                ESP_LOGI("CONTDOWN_SW4", "%ld ", (long)cnt_timestamp);
                ex_info.sw_countdown[3] = cnt_timestamp;
                if(ex_info.sw_type[3]){
                app_time_countdown_handle(4,ex_info.sw_countdown[3]);
                }
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
                break;
            }
            case CMD_EXCONFIG_SW1_CTRL_MODE:
            case CMD_EXCONFIG_SW2_CTRL_MODE:
            case CMD_EXCONFIG_SW3_CTRL_MODE:
            case CMD_EXCONFIG_SW4_CTRL_MODE: {
                cmd_control_mode_handle(param->model_operation.msg, param->model_operation.length);
                build_extra_res_sucess_cod(param->model_operation.msg, param->model_operation.length, 0x00);
            break;
            }
            case CMD_EXCONFIG_NIGHT_ENB:{
                cmd_night_mode_parse_data(param->model_operation.msg, param->model_operation.length);
                Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG_STT,param->model_operation.msg, param->model_operation.length);
            break;
            }
        default:
            break;
        }
    } 
    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG){
        ESP_LOGI(TAG,"-----------------------------------------------------Get extra config command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        cmd_get_extra_config_handle(param->model_operation.msg, param->model_operation.length);
    }
    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEADD){
        ESP_LOGI(TAG,"-----------------------------------------------------Add schedule command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        cmd_add_schedule(param->model_operation.msg, param->model_operation.length, &schedule_list[mesh_schedule_count]);
        buid_res_add_schedule(param->model_operation.msg, msgPointer , 0);
        Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEADD_STT, msg, msgPointer);
        reset_pointer();
    }
    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEDELETE){
        ESP_LOGI(TAG,"-----------------------------------------------------Delete schedule command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        uint8_t dele_msg = param->model_operation.msg[0];
        if(dele_msg == 0x60){
            memset(schedule_list, 0, mesh_schedule_count);
            mesh_schedule_count = 0; 
            Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEDELETE_STT, &dele_msg, 1);
        }
    }
     else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SCHEDULELIST){
        ESP_LOGI(TAG,"-----------------------------------------------------List schedule command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
    }    
    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_GETDEVICEINFO){
        ESP_LOGI(TAG,"-----------------------------------------------------Get device command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        build_res_cmd_getDevice();
        ESP_LOG_BUFFER_HEX(TAG,msg, msgPointer);
        Res_vendor_modal(TOPIC_DEVICE_PUB, ESP_BLE_MESH_VND_MODEL_OP_GETDEVICEINFO_STT, msg, msgPointer);
        reset_pointer();
    }
    else if(param-> model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_STARTOTA){
        ESP_LOGI(TAG,"-----------------------------------------------------Start OTA command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        start_ota_handle(param->model_operation.msg, param->model_operation.length);
    }
    else if(param-> model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SETWIFIINFO){
        ESP_LOGI(TAG,"-----------------------------------------------------Set wifi command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        cmd_set_wifi_handle(param->model_operation.msg, param->model_operation.length);
    }
    else if(param-> model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_SETENDPOINTCONFIG ){
        ESP_LOGI(TAG,"-----------------------------------------------------Set endpoint command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        cmd_set_endpoint_handle(param->model_operation.msg, param->model_operation.length);
    }
    else if(param->model_operation.opcode == ESP_BLE_MESH_VND_MODEL_OP_DELETEENDPOINTCONFIG){
        ESP_LOGI(TAG,"-----------------------------------------------------Delete endpoint command-----------------------------------------------------");
        ESP_LOG_BUFFER_HEX(TAG, param->model_operation.msg, param->model_operation.length);
        cmd_delete_endpoint_handle(param->model_operation.msg, param->model_operation.length);
    }
    else {
        uint32_t opcode = param->model_operation.opcode;
        uint16_t length = param->model_operation.length;
        const uint8_t *data = param->model_operation.msg;
        ESP_LOGI(TAG, "Opcode: 0x%06" PRIx32, opcode);
        if (length > 0) {
        ESP_LOG_BUFFER_HEX(TAG, data, length);
        } else {
        ESP_LOGI(TAG, "No payload");
        }
    }
    break;
    }
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code) {
            ESP_LOGE(TAG, "Failed to send message 0x%06" PRIx32, param->model_send_comp.opcode);
            break;
        }
        ESP_LOGI(TAG, "Send 0x%06" PRIx32, param->model_send_comp.opcode);
        break;
    
    default:
        break;
} 
}

static void ble_mesh_time_model_cb(esp_ble_mesh_time_scene_server_cb_event_t event, esp_ble_mesh_time_scene_server_cb_param_t *param)
{ 
    switch(event){ 
        case ESP_BLE_MESH_TIME_SCENE_SERVER_RECV_GET_MSG_EVT: 
        break; 
        case ESP_BLE_MESH_TIME_SCENE_SERVER_RECV_SET_MSG_EVT: 
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_TIME_SET) set_time_TAI_to_UNIX(param->value.set.time.tai_seconds, param->value.set.time.tai_utc_delta);
        break;
    default:
        break;
    }
}        

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err;

    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(ble_mesh_generic_modal_cb);
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);
    esp_ble_mesh_register_time_scene_server_callback(ble_mesh_time_model_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize mesh stack");
        return err;
    }

    esp_ble_mesh_set_unprovisioned_device_name("VCONNEX_MESH_AA14");
    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable mesh node");
        return err;
    }
    ESP_LOGI(TAG, "BLE Mesh Node initialized");
    return ESP_OK;
}

void ble_mesh_config(void)
{
    esp_err_t err;
    ESP_LOGI(TAG, "BLE mesh is initializing...");
    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }
    uint8_t* uuid = app_ble_mesh_get_uuid();
    ble_mesh_get_dev_uuid(uuid);
    
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase_partition("mesh_core"));
        ESP_ERROR_CHECK(nvs_flash_init_partition("mesh_core"));
    }

    err = ble_mesh_init();
    if (err) {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }
    esp_ble_mesh_model_subscribe_group_addr(esp_ble_mesh_get_primary_element_address(), 0xffff, ESP_BLE_MESH_MODEL_ID_TIME_SETUP_SRV, BLEMESH_TOPIC_TIME_SERVER_SUB);
}

uint8_t* app_wifi_get_mac_uint() {
  if(pu8Mac[0] == 0) {
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(pu8Mac));
     printf("WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               pu8Mac[0], pu8Mac[1], pu8Mac[2],
               pu8Mac[3], pu8Mac[4], pu8Mac[5]);
    }
  return pu8Mac;
}

uint8_t* app_ble_mesh_get_uuid(){
  uint8_t *wifiMac = app_wifi_get_mac_uint();
  uint8_t dev_uuidIndex = 2+BD_ADDR_LEN;
  memcpy(dev_uuid + 2, wifiMac, BD_ADDR_LEN);

  dev_uuid[dev_uuidIndex] =(uint8_t)(DEVICE_TYPE>>8);
  dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] =(uint8_t)(DEVICE_TYPE&0xff);
  dev_uuidIndex++;
  const uint16_t *uVer = uintCurrentVer;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[0]>>8 & 0xff);   dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[0]& 0xff);    dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[1]>>8 & 0xff);  dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[1]& 0xff);    dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[2]>>8 &0xff);  dev_uuidIndex++;
  dev_uuid[dev_uuidIndex] = (uint8_t)(uVer[2]& 0xff);  dev_uuidIndex++;
  return dev_uuid;
}

esp_err_t appBleMesh_bindAppKey(){
    esp_err_t err = ESP_OK;
	for (int j=0;j<esp_ble_mesh_get_element_count();j++){
		for (int i=0;i<elements[j].sig_model_count;i++){
			ESP_LOGI(TAG ,"Bind element %d  sigModel %d  companyId 0x%4x  modelId 0x%4x  appIdx %d",j,i,0xffff,elements[j].sig_models[i].model_id,0);
			err = esp_ble_mesh_node_bind_app_key_to_local_model(elements[j].element_addr, 0xffff, elements[j].sig_models[i].model_id, 0);
			delay_ms(100);
		}
		for (int i=0;i<elements[j].vnd_model_count;i++){
			ESP_LOGI(TAG ,"Bind element %d  vendorModel %d  companyId 0x%4x  modelId 0x%4x  appIdx %d",j,i,
					elements[j].vnd_models[i].vnd.company_id, elements[j].vnd_models[i].vnd.model_id,0);
			err = esp_ble_mesh_node_bind_app_key_to_local_model( elements[j].element_addr, elements[j].vnd_models[i].vnd.company_id , elements[j].vnd_models[i].vnd.model_id, 0);
			
			delay_ms(100);
		}
	}
    return err;
}

void BLE_mesh_response_to_gw(uint8_t element, uint16_t add, uint8_t value){
	esp_ble_mesh_msg_ctx_t ctx;
	ctx.net_idx = 0;
	ctx.app_idx = 0;
	ctx.addr = add;
	ctx.send_ttl = 7;
	ctx.send_rel = 0;
    if(element == 4) {
        esp_ble_mesh_elem_t *elem = esp_ble_mesh_find_element((esp_ble_mesh_get_primary_element_address()));
        for (int i = 0; i < esp_ble_mesh_get_element_count(); i++) {
        esp_ble_mesh_model_t *model = esp_ble_mesh_find_sig_model(&elem[i],ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV);
        if (model) {
            esp_ble_mesh_server_model_send_msg(model, &ctx, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,1, &value);
            }
        }
    }
	else if (element==0) esp_ble_mesh_server_model_send_msg(onoff_server_data_0.model,&ctx,ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,1,&value);
	else if (element==1) esp_ble_mesh_server_model_send_msg(onoff_server_data_1.model,&ctx,ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,1,&value);
	else if (element==2) esp_ble_mesh_server_model_send_msg(onoff_server_data_2.model,&ctx,ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,1,&value);
	else if (element==3) esp_ble_mesh_server_model_send_msg(onoff_server_data_3.model,&ctx,ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,1,&value);
}

void Res_vendor_modal(uint16_t add,uint32_t opcode, uint8_t *data, uint8_t len){
	esp_ble_mesh_msg_ctx_t ctx = {0};
	esp_err_t err;
	ctx.addr = add;
	ctx.send_ttl = 7;
	err = esp_ble_mesh_server_model_send_msg(&vnd_models[0],&ctx,opcode,len,data);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "Failed to send vendor message 0x%08" PRIx32, opcode);
	}
}

void res_ok_vendor(uint16_t add, uint32_t opcode){
	uint8_t data_res[1] = {0};
	uint8_t len = 1;
	Res_vendor_modal(add,opcode,data_res,len);
	delay_ms(100);
	Res_vendor_modal(add,opcode,data_res,len);
}


void ble_stack_deinit(void) {
    ESP_LOGI("Ble_erase","Deiniting ble stack");
    esp_err_t err;
    esp_ble_mesh_deinit_param_t deinit_param = {
    .erase_flash = true,  
    };
    
    err = esp_ble_mesh_deinit(&deinit_param);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to de-initialize BLE Mesh: %s", esp_err_to_name(err));
    }

    err = esp_bluedroid_disable();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to disable bluedroid: %s", esp_err_to_name(err));
    }
    err = esp_bluedroid_deinit();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to de-initialize bluedroid: %s", esp_err_to_name(err));
    }
    err = esp_bt_controller_disable();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to disable BT controller: %s", esp_err_to_name(err));
    }
    err = esp_bt_controller_deinit();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to de-initialize BT controller: %s", esp_err_to_name(err));
    }
}

void mesh_flag_save(){
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return;
    nvs_set_u8(nvs, "mesh_flag", mesh_mode);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void mesh_load_config(){
    nvs_handle_t nvs;
    size_t size;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if(err != ESP_OK) return;

    nvs_get_u8(nvs, "mesh_flag", &mesh_mode);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "mesh_mode = %d", mesh_mode);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "mesh_flag chưa tồn tại trong NVS");
    } else {
        ESP_LOGE(TAG, "Lỗi đọc NVS: %s", esp_err_to_name(err));
    }
    nvs_close(nvs);
}

void cmd_add_schedule(uint8_t *data, size_t len, schedule_item_t *item) {
    size_t i = 0;
    while (i < len) {
        uint8_t tlv_id = data[i++];
        if (i >= len) break;
        uint8_t l = data[i++];
        if (i + l > len) break;
        switch (tlv_id) {
            case 0x05: // id
                if (l == 4) {
                    item->id = ((uint32_t)data[i]   << 24) | ((uint32_t)data[i+1] << 16)|((uint32_t)data[i+2] << 8)|((uint32_t)data[i+3]);
                    printf("Add schedule id %u\n", (unsigned)item->id);
                }
                break;
            case 0x0B: 
                if (l == 8) {
                    item->timestamp = 0;
                    for (int b = 0; b < 8; b++) {
                        item->timestamp |= ((uint64_t)data[i+b] << (8*(l-1-b)));
                    }
                    ESP_LOGI("MESH_SCHEDULE", "Timestamp = %lld", item->timestamp);
                }
                break;

            case 0x0C: // loop
                if (l == 1) item->loop = data[i];
                break;

            case 0x0E: // switch_1
                if (l == 1) item->dev_states[0] = data[i];
                break;

            case 0x0F: // switch_2
                if (l == 1) item->dev_states[1] = data[i];
                break;

            case 0x10: // switch_3
                if (l == 1) item->dev_states[2] = data[i];
                break;

            case 0x11: // switch_4
                if (l == 1) item->dev_states[3] = data[i];
                break;

            default:
                break;
        }
        i += l;
    }
    Istime(*item);
    mesh_schedule_count++;
    printf("Schedule list: ");
    for (int k = 0; k < mesh_schedule_count; k++) {
        printf(" %u ", (unsigned)schedule_list[k].id);
    }
    printf("\n");
}