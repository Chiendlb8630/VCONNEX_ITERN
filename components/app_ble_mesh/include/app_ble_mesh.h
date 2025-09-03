#pragma once

#include "esp_log.h"
#include "esp_system.h"
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

typedef struct {
    uint8_t buzzer_enb;
    uint8_t liNess;
    uint8_t rgbon;
    uint8_t save_status;

    uint32_t sw_countdown[4];
    uint8_t sw_ctrl_mode[4];
    uint8_t sw_led_off[4];
    uint8_t sw_type[4];

    uint32_t night_enb;
    uint32_t night_begin;
    uint32_t night_end;
} extraConf_info;


extern uint8_t mesh_mode;
extern uint16_t save_endpoint_ele[4];
extern EventGroupHandle_t deinit_event_group;

extern uint8_t msg[250];
extern uint8_t dev_uuid[16];
extern uint8_t pu8Mac[6];
extern uint16_t save_endpoint_ele[4];
extern uint16_t uintCurrentVer[3];
extern uint8_t mesh_mode;
extern uint8_t msgPointer;
extern extraConf_info ex_info;


void build_res_cmd_getDevice();
void ble_mesh_config(void);
void BLE_mesh_response_to_gw(uint8_t element,uint16_t add, uint8_t value);
void mesh_flag_save();
void mesh_load_config();
void ble_stack_deinit(void);
void mesh_send_status();
void send_onoff_set(uint8_t element, uint16_t dst_elem_addr, uint8_t onoff);
esp_err_t endpoint_link_load(void);

esp_err_t endpoint_link_save(void);
void cmd_set_wifi_handle(const uint8_t *info, size_t info_len);
void start_ota_handle(const uint8_t *info , size_t info_len);
void cmd_control_mode_handle(uint8_t * msg, size_t len);

void cmd_set_endpoint_handle(const uint8_t *info, size_t info_len);
void save_endpoint_config(uint8_t index, uint16_t endpoint_ele);
void cmd_delete_endpoint_handle(const uint8_t *msg, size_t len);

void buid_res_add_schedule(uint8_t *msg, size_t len, uint8_t err_code);

 void add_arr_type_msg(uint8_t *msg, uint8_t ParamId, uint8_t len, uint8_t *arr);	
 void add_str_type_msg(uint8_t *msg, uint8_t ParamId , uint8_t len, char *str);    
 void add_value_type_msg(uint8_t *msg, uint8_t ParamId,  uint8_t type, uint64_t value);

void build_res_get_extra_config();
void build_extra_res_sucess_cod(uint8_t *model_mes, uint8_t len, uint8_t error_code);
void reset_pointer();
void delay_ms(int ms);

void cmd_get_extra_config_handle(uint8_t* cmd, size_t len);
void Res_vendor_modal(uint16_t add,uint32_t opcode, uint8_t *data, uint8_t len);

void cmd_night_mode_parse_data(uint8_t* cmd, size_t len);   
void cmd_extra_set_sw_type(uint8_t* cmd);
void cmd_set_color_on_handle(uint8_t* cmd);
void cmd_set_color_off_handle(uint8_t* cmd);

typedef struct {
    uint16_t app_key;
    uint16_t net_key;
}mesh_info_t; 

typedef struct {
    char ssid[33];      
    char password[65]; 
    char url[128];
    char firm[10]; 
    bool valid;         
} wifi_ble_mesh_ota;
extern wifi_ble_mesh_ota ble_mesh_wifi;

#define DEVT        3018
#define CID_ESP     0x02E5
#define EVENT_DEINIT_BLE   BIT0

#define ESP_BLE_MESH_VND_MODEL_ID_SERVER_CONTROL    0x11        // Control modal ID 
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER_CONFIG     0x0001        // Config modal ID
#define TOPIC_DEVICE_PUB                            0xF000    
#define BLEMESH_TOPIC_TIME_SERVER_SUB		        0xF001  
#define DEVICE_TYPE                                 3018        
#define EPOCH_OFFSET                                946684800UL  

#define ESP_BLE_MESH_VND_MODEL_OP_SET_DATA      ESP_BLE_MESH_MODEL_OP_3(0x0F, 0x01) // SET DATA CONTROL OPCODE
#define ESP_BLE_MESH_VND_MODEL_OP_GET_DATA      ESP_BLE_MESH_MODEL_OP_3(0x11, 0x01) // GET DATA CONTROL OPCODE

#define CMDMESH_GET_DEVICE_INFO 		                    0x10
#define ESP_BLE_MESH_VND_MODEL_OP_GETDEVICEINFO 			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_DEVICE_INFO, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_GETDEVICEINFO_STT 		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_DEVICE_INFO+1, CID_ESP)

#define CMDMESH_START_OTA 				                    0x1C
#define ESP_BLE_MESH_VND_MODEL_OP_STARTOTA	 				ESP_BLE_MESH_MODEL_OP_3(CMDMESH_START_OTA, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_STARTOTA_STT				ESP_BLE_MESH_MODEL_OP_3(CMDMESH_START_OTA+1,CID_ESP)

#define CMDMESH_SET_WIFI_INFO 			                    0x1A
#define ESP_BLE_MESH_VND_MODEL_OP_SETWIFIINFO	 			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_WIFI_INFO, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SETWIFIINFO_STT			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_WIFI_INFO+1, CID_ESP)

//---------------------------------------------CMD_CONFIG-----------------------------------------------------------------
#define CMDMESH_SET_EXTRACONFIG 		                    0x16     //  SET EXTRA CONFIG (1)
#define ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG 			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_EXTRACONFIG, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SETEXTRACONFIG_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_EXTRACONFIG+1, CID_ESP)

#define CMD_EXCONFIG_BUZ_ENB                                0x03
#define CMD_EXCONFIG_LED_ENB                                0x04
#define CMD_EXCONFIG_RGBON                                  0x05
#define CMD_EXCONFIG_RGBOFF                                 0x06
#define CMD_EXCONFIG_LIGHTNESS                              0x27
#define CMD_EXCONFIG_SAVE_STATE                             0x28

#define CMD_EXCONFIG_SW1_COUNTDOWN                          0x0B
#define CMD_EXCONFIG_SW2_COUNTDOWN                          0x0C
#define CMD_EXCONFIG_SW3_COUNTDOWN                          0x0D
#define CMD_EXCONFIG_SW4_COUNTDOWN                          0x0E

#define CMD_EXCONFIG_NIGHT_ENB                              0x23
#define CMD_EXCONFIG_NIGHT_BEGIN                            0x24
#define CMD_EXCONFIG_NIGHT_END                              0x25
#define CMD_EXCONFIG_NIGHT_TZ                               0x26

#define CMD_EXCONFIG_SW1_CTRL_MODE	                        0x0F
#define CMD_EXCONFIG_SW2_CTRL_MODE	                        0x10
#define CMD_EXCONFIG_SW3_CTRL_MODE	                        0x11
#define CMD_EXCONFIG_SW4_CTRL_MODE                          0x12

#define CMD_EXCONFIG_SW1_TYPE                               0x07
#define CMD_EXCONFIG_SW2_TYPE                               0x08 
#define CMD_EXCONFIG_SW3_TYPE                               0x09
#define CMD_EXCONFIG_SW4_TYPE                               0x0A

#define CMD_EXCONFIG_SW1_LED_OFF 	                        0x13
#define CMD_EXCONFIG_SW2_LED_OFF 	                        0x14
#define CMD_EXCONFIG_SW3_LED_OFF 	                        0x15
#define CMD_EXCONFIG_SW4_LED_OFF 	                        0x16


#define CMDMESH_GET_EXTRACONFIG 		                    0x18       // GET EXTRA CONFIG (2) 
#define ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG 			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_EXTRACONFIG, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_GETEXTRACONFIG_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_EXTRACONFIG+1, CID_ESP)
//-------------------------------------------------------------------------------------------------------------------------
#define CMDMESH_SET_ENDPOINT_CONFIG 	                    0x20    // SET ENDPOINT (3)
#define ESP_BLE_MESH_VND_MODEL_OP_SETENDPOINTCONFIG	 		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_ENDPOINT_CONFIG, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SETENDPOINTCONFIG_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SET_ENDPOINT_CONFIG+1, CID_ESP)

#define CHANNEL_1   0x04
#define CHANNEL_2   0x05
#define CHANNEL_3   0x06
#define CHANNEL_4   0x07

#define CMDMESH_GET_ENDPOINT_CONFIG 	                    0x1E   // GET ENDPOINT (4)
#define ESP_BLE_MESH_VND_MODEL_OP_GETENDPOINTCONFIG			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_ENDPOINT_CONFIG, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_GETENDPOINTCONFIG_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_GET_ENDPOINT_CONFIG+1, CID_ESP) 

#define CMDMESH_DELETE_ENDPOINT_CONFIG 	                    0x22   // DELETE ENDPOINT (5)
#define ESP_BLE_MESH_VND_MODEL_OP_DELETEENDPOINTCONFIG		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_DELETE_ENDPOINT_CONFIG, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_DELETEENDPOINTCONFIG_STT	ESP_BLE_MESH_MODEL_OP_3(CMDMESH_DELETE_ENDPOINT_CONFIG+1, CID_ESP)
//--------------------------------------------------------------------------------------------------------------------------
#define CMDMESH_SCHEDULE_ADD 			                    0x28    // TIMER (7)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEADD				ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_ADD, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEADD_STT			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_ADD+1, CID_ESP)

#define SCHEDULE_ID                                         0x05                                   
#define EX_TIME                                             0x0B
#define SWITCH_1_PARAM                                      0x0E
#define SWITCH_2_PARAM                                      0x0F
#define SWITCH_3_PARAM                                      0x10
#define SWITCH_4_PARAM                                      0x11

#define CMDMESH_SCHEDULE_DELETE			                    0x2A    // TIMER (8)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEDELETE			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_DELETE, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEDELETE_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_DELETE+1, CID_ESP)

#define CMDMESH_SCHEDULE_LIST 			                    0x2C    // TIMER (9)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULELIST				ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_LIST, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULELIST_STT			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_LIST+1, CID_ESP)

#define CMDMESH_SCHEDULE_GET			                    0x2E      // TIMER(10)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEGET				ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_GET, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEGET_STT			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_GET+1, CID_ESP)

#define CMDMESH_SCHEDULE_UPDATE			                    0x30       // TIMER(11)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEUPDATE			ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_UPDATE, CID_ESP)
#define ESP_BLE_MESH_VND_MODEL_OP_SCHEDULEUPDATE_STT		ESP_BLE_MESH_MODEL_OP_3(CMDMESH_SCHEDULE_UPDATE+1, CID_ESP)
//---------------------------------------------------------------------------------------------------------------------------------


#define U8 1
#define U16 2
#define I32 3
#define By 4
#define Bo 5

#define POINTER_TAG "MSG POINTER"
#define WIFI_TAG "CMD_WIFI"