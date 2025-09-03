#pragma once 
#include "led_strip.h"
#define BUZZER_GPIO 12
#define LED_COLOR_RED   1
#define LED_COLOR_BLUE  2
#define ON              1
#define OFF             0
#define PRESSED_TIME 3000
#define MESH_TIME 5000

typedef struct {
    uint8_t rOn;
    uint8_t bOn;
    uint8_t gOn;

    uint8_t rOff;
    uint8_t bOff;
    uint8_t gOff;
}set_color_info_t; 

extern uint8_t touch_bits[];
extern uint8_t prev_touch;
extern led_strip_handle_t led_strips[4];
extern int control_mode_msk[4];
extern bool led_states[4];
extern int led_count;
extern bool buzzer_enable;
extern bool led_enable;
extern bool state_save_on;
extern int duty;
extern bool lnIsSet;
extern set_color_info_t color_info;
extern int hold_counters[4];

void run_task(void *pvParameters);
void buzzer_init(void);
void print_led_state(void);
void buzzer_beep(int ms);
void led_set_color(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b);
void switch_init_led(void);
void load_state(void);
void load_state_from_nvs(void);
void save_states_to_nvs(void);
void set_led(void);
void set_led_all_on(void);
void set_led_all_off(void);
void pair_beep(int duration_ms, int pause_ms);
void led_blink_color( uint8_t color, int times, int on_ms, int off_ms);
void set_duty_all_led();
void relay_gpio_init(void);
void relay_set_state(uint8_t index, bool state);
void switch_change_state(uint8_t idx, uint8_t onoff);

void wifi_pair_mode();
void ble_mesh_pair_mode();

