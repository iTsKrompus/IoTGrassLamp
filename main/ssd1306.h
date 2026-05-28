#ifndef SSD1306_H
#define SSD1306_H

#include "driver/i2c_master.h"
#include "esp_err.h"


#include <stdint.h>

// Comandos de control
#define OLED_CONTROL_BYTE_CMD 0x00
#define OLED_CONTROL_BYTE_DATA 0x40

// Comandos fundamentales
#define OLED_CMD_SET_CONTRAST_CONTROL 0x81
#define OLED_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define OLED_CMD_DISPLAY_ALL_ON 0xA5
#define OLED_CMD_NORMAL_DISPLAY 0xA6
#define OLED_CMD_INVERT_DISPLAY 0xA7
#define OLED_CMD_DISPLAY_OFF 0xAE
#define OLED_CMD_DISPLAY_ON 0xAF

// Direccionamiento
#define OLED_CMD_SET_MEMORY_ADDR_MODE 0x20
#define OLED_CMD_SET_COLUMN_ADDR 0x21
#define OLED_CMD_SET_PAGE_ADDR 0x22

// Configuración Hardware
#define OLED_CMD_SET_DISPLAY_START_LINE 0x40
#define OLED_CMD_SET_SEGMENT_REMAP 0xA0
#define OLED_CMD_SET_MUX_RATIO 0xA8
#define OLED_CMD_SET_COM_SCAN_MODE 0xC0
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3
#define OLED_CMD_SET_COM_PIN_MAP 0xDA
#define OLED_CMD_NOP 0xE3

// Tiempos y carga
#define OLED_CMD_SET_DISPLAY_CLK_DIV 0xD5
#define OLED_CMD_SET_PRECHARGE 0xD9
#define OLED_CMD_SET_VCOMH_DESELCT 0xDB
#define OLED_CMD_SET_CHARGE_PUMP 0x8D

// Inicializa la pantalla usando el BUS que tú creaste en i2c_init.c
void ssd1306_init(i2c_master_bus_handle_t bus_handle);

// Funciones para pintar
void ssd1306_clear(void);
void ssd1306_print_text(int x, int y, char *str);
void ssd1306_update(void); // Envía el buffer a la pantalla
void ssd1306_draw_hline(int x, int y, int w);

#endif // SSD1306_H