#include "oled_ui.h"
#include "ssd1306.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#include "lamp.h"
#include "schedule.h"
#include "esp_log.h"
#include "power.h"
#include "math.h"

// estado de la UI
typedef enum {
    UI_STATE_MAIN,
    UI_STATE_CONNECTING,
    UI_STATE_AP
} ui_state_t;

static ui_state_t current_state = UI_STATE_CONNECTING;
static char current_wifi_ssid[32] = "Unknown";
static char current_ap_ip[16] = "0.0.0.0";

static bool show_scheduling_indicator = false;

void oled_ui_set_scheduling_mode(bool scheduling){
    show_scheduling_indicator = scheduling;
}

void oled_ui_set_mode_main(void) {
    current_state = UI_STATE_MAIN;
}

void oled_ui_set_mode_connecting(const char* ssid) {
    strncpy(current_wifi_ssid, ssid, sizeof(current_wifi_ssid) - 1);
    current_state = UI_STATE_CONNECTING;
}

void oled_ui_set_mode_ap(const char* ip) {
    strncpy(current_ap_ip, ip, sizeof(current_ap_ip) - 1);
    current_state = UI_STATE_AP;
    vTaskDelay(pdMS_TO_TICKS(10000));
}

static void draw_main_screen(void) {
    int mode = lamp_get_mode();
    int bri  = lamp_get_brightness();
    int start = schedule_get_start_time();
    int end   = schedule_get_end_time();
    float power = power_get_watts();

    char buff[64];
    ssd1306_clear();
    
    sprintf(buff, "Mode: %s", (mode == 1) ? "Red" : (mode == 2) ? "White" : "Red White");
    ssd1306_print_text(0, 0, buff);

    sprintf(buff, "Bri: %d%%  %.2fW", bri, fabs(power));
    ssd1306_print_text(0, 2, buff);
    
    ssd1306_draw_hline(0, 28, 128);

    if (start != -1 && end != -1) {
        sprintf(buff, "Sch: %02d:00 - %02d:00%s", start, end, show_scheduling_indicator ? "*" : "");
        ssd1306_print_text(0, 4, buff);
    } else {
        sprintf(buff, "Sch: Disabled%s", show_scheduling_indicator ? "*" : "");
        ssd1306_print_text(0, 4, buff);
    }

    ssd1306_update();
}

static void draw_connecting_screen(void) {
    ssd1306_clear();
    ssd1306_print_text(2, 1, "Connecting to:");
    ssd1306_print_text(2, 3, current_wifi_ssid);
    
    static int dots = 0;
    char dot_str[5] = "";
    for(int i=0; i<dots; i++) strcat(dot_str, ".");
    ssd1306_print_text(60, 5, dot_str);
    
    dots = (dots + 1) % 4;

    ssd1306_update();
}

static void draw_ap_screen(void) {
    ssd1306_clear();
    ssd1306_print_text(0, 0, "Connection Failed!");
    ssd1306_print_text(0, 2, "WiFi Setup Mode:");
    ssd1306_print_text(0, 4, "Connect to IoTLamp");
    ssd1306_print_text(0, 6, current_ap_ip);
    ssd1306_update();
}

static void oled_task_entry(void *arg) {
    ESP_LOGI("OLED", "UI Task iniciada");
    
    while (1) {
        switch (current_state) {
            case UI_STATE_MAIN:
                draw_main_screen();
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case UI_STATE_CONNECTING:
                draw_connecting_screen();
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case UI_STATE_AP:
                draw_ap_screen();
                vTaskDelay(pdMS_TO_TICKS(2000));
                break;
        }
    }
}

void oled_ui_init(i2c_master_bus_handle_t i2c_bus) {
    ssd1306_init(i2c_bus);
    xTaskCreatePinnedToCore(oled_task_entry, "oled_ui_task", 4096, NULL, 5, NULL, 1);
}
