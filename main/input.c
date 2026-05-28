#include "input.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lamp.h"
#include "schedule.h"
#include "oled_ui.h"

#define BUTTON_1_GPIO 18
#define BUTTON_2_GPIO 19
#define BUTTON_3_GPIO 5

static const char *TAG = "INPUT";

// estados del input
typedef enum {
    INPUT_MODE_LIGHT,
    INPUT_MODE_SCHEDULE
} input_mode_t;

static input_mode_t current_input_mode = INPUT_MODE_LIGHT;

static void input_task(void *arg) {
    while (1) {
        
        // MODO
        if (gpio_get_level(BUTTON_3_GPIO) == 0) {
            if (current_input_mode == INPUT_MODE_LIGHT) {
                current_input_mode = INPUT_MODE_SCHEDULE;
                oled_ui_set_scheduling_mode(true);
                ESP_LOGW(TAG, ">>> MODO INPUT CAMBIADO: EDICIÓN DE HORARIO <<<");
            } else {
                current_input_mode = INPUT_MODE_LIGHT;
                oled_ui_set_scheduling_mode(false);
                ESP_LOGW(TAG, ">>> MODO INPUT CAMBIADO: CONTROL DE LUZ <<<");
            }

            // Debounce
            vTaskDelay(pdMS_TO_TICKS(50));
            while(gpio_get_level(BUTTON_3_GPIO) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // BOTÓN 1 (Color y hora de fin)
        if (gpio_get_level(BUTTON_1_GPIO) == 0) {
            
            switch (current_input_mode) {
                case INPUT_MODE_LIGHT:
                    // Acción A: Cambiar Modo de Color
                    ESP_LOGI(TAG, "Btn 1 (Luz): Cambiando Color");
                    uint8_t m = lamp_get_mode();
                    m++;
                    if (m > 3) m = 1;
                    lamp_set_mode(m);
                    break;

                case INPUT_MODE_SCHEDULE:
                    // Acción B: Cambiar Hora de Fin
                    ESP_LOGI(TAG, "Btn 2 (Horario): Cambiando Fin");
                    int end_h = schedule_get_end_time();
                    end_h++;
                    if (end_h > 23) end_h = 0;
                    schedule_set_end_time(end_h);
                    break;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
            while(gpio_get_level(BUTTON_1_GPIO) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        // BOTÓN 2 (Brillo y hora de inicio)
        if (gpio_get_level(BUTTON_2_GPIO) == 0) {
            
            switch (current_input_mode) {
                case INPUT_MODE_LIGHT:
                    ESP_LOGI(TAG, "Btn 2 (Luz): Cambiando Brillo");
                    uint8_t b = lamp_get_brightness();
                    int next_b = b + 10;
                    if (next_b > 100) next_b = 0;
                    lamp_set_brightness((uint8_t)next_b);
                    break;
                case INPUT_MODE_SCHEDULE:
                    ESP_LOGI(TAG, "Btn 1 (Horario): Cambiando Inicio");
                    int start_h = schedule_get_start_time();
                    start_h++;
                    if (start_h > 23) start_h = 0;
                    schedule_set_start_time(start_h);
                    break;
            }

            vTaskDelay(pdMS_TO_TICKS(50)); 
            while(gpio_get_level(BUTTON_2_GPIO) == 0) vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void input_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1_GPIO) | (1ULL << BUTTON_2_GPIO) | (1ULL << BUTTON_3_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // Resistencias Pull-Up activas
        .pull_down_en = GPIO_PULLDOWN_DISABLE, 
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&io_conf);

    xTaskCreate(input_task, "input_task", 4096, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Sistema Input Iniciado. Estado inicial: LIGHT CONTROL");
}