#include <string.h>
#include "ssd1306.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "font6x8.h" // <--- CAMBIO 1: Usamos la nueva fuente

#include "oled_ui.h"

static const char *TAG = "SSD1306";
static i2c_master_dev_handle_t dev_handle;
static uint8_t buffer[128 * 64 / 8];

// --- FUNCIONES INTERNAS (iguales) ---

static esp_err_t write_cmd(uint8_t cmd) {
    uint8_t data[] = {OLED_CONTROL_BYTE_CMD, cmd};
    return i2c_master_transmit(dev_handle, data, sizeof(data), -1);
}

static esp_err_t write_data(const uint8_t *data, size_t len) {
    uint8_t *temp_buf = heap_caps_malloc(len + 1, MALLOC_CAP_DMA);
    if (!temp_buf) return ESP_ERR_NO_MEM;
    
    temp_buf[0] = OLED_CONTROL_BYTE_DATA;
    memcpy(temp_buf + 1, data, len);
    
    esp_err_t ret = i2c_master_transmit(dev_handle, temp_buf, len + 1, -1);
    free(temp_buf);
    return ret;
}

// --- FUNCIONES PÚBLICAS ---

void ssd1306_init(i2c_master_bus_handle_t bus_handle) {
    ESP_LOGI(TAG, "Configurando dispositivo SSD1306...");
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = 0x3C,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    write_cmd(OLED_CMD_DISPLAY_OFF);
    write_cmd(OLED_CMD_SET_MUX_RATIO); write_cmd(0x3F);
    write_cmd(OLED_CMD_SET_DISPLAY_OFFSET); write_cmd(0x00);
    write_cmd(OLED_CMD_SET_DISPLAY_START_LINE);
    write_cmd(OLED_CMD_SET_SEGMENT_REMAP | 0x01);
    write_cmd(OLED_CMD_SET_COM_SCAN_MODE | 0x08);
    write_cmd(OLED_CMD_SET_COM_PIN_MAP); write_cmd(0x12);
    write_cmd(OLED_CMD_SET_CONTRAST_CONTROL); write_cmd(0xFF);
    write_cmd(OLED_CMD_DISPLAY_ALL_ON_RESUME);
    write_cmd(OLED_CMD_NORMAL_DISPLAY);
    write_cmd(OLED_CMD_SET_DISPLAY_CLK_DIV); write_cmd(0x80);
    write_cmd(OLED_CMD_SET_CHARGE_PUMP); write_cmd(0x14);
    write_cmd(OLED_CMD_SET_MEMORY_ADDR_MODE); write_cmd(0x00);
    write_cmd(OLED_CMD_DISPLAY_ON);
    
    ssd1306_clear();
    ssd1306_update();
}

void ssd1306_clear(void) {
    memset(buffer, 0, sizeof(buffer));
}

// --- CAMBIO 2: Lógica adaptada para 6 pixeles de ancho ---
void ssd1306_print_text(int x, int y, char *str) {
    if (y > 7) return;

    while (*str) {
        if (x >= 128) break;
        uint8_t c = *str;

        if (c >= 32 && c <= 127) {
            int font_idx = c - 32;
            
            // Bucle solo hasta 6 (ancho de la nueva fuente)
            for (int i = 0; i < 6; i++) {
                if (x + i < 128) {
                    int buffer_idx = (y * 128) + (x + i);
                    buffer[buffer_idx] = font6x8[font_idx][i];
                }
            }
        }
        // Avanzamos solo 6 píxeles (letra más estrecha)
        x += 6; 
        str++;
    }
}

// Función de línea horizontal (la añadimos antes)
void ssd1306_draw_hline(int x, int y, int w) {
    if (y < 0 || y >= 64) return;
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    uint8_t mask = (1 << bit);

    for (int i = 0; i < w; i++) {
        int col = x + i;
        if (col >= 0 && col < 128) {
            int idx = (page * 128) + col;
            buffer[idx] |= mask;
        }
    }
}

void ssd1306_update(void) {
    write_cmd(OLED_CMD_SET_COLUMN_ADDR); write_cmd(0); write_cmd(127);
    write_cmd(OLED_CMD_SET_PAGE_ADDR); write_cmd(0); write_cmd(7);
    write_data(buffer, sizeof(buffer));
}