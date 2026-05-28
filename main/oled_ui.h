#ifndef OLED_UI_H
#define OLED_UI_H

#include "driver/i2c_master.h"

void oled_ui_init(i2c_master_bus_handle_t i2c_bus);

void oled_ui_set_mode_main(void);

void oled_ui_set_mode_connecting(const char* ssid);

void oled_ui_set_mode_ap(const char* ip);

void oled_ui_set_scheduling_mode(bool scheduling);

#endif // OLED_UI_H