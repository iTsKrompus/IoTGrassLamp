#include "lamp.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "persistence.h"


#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_FREQUENCY          5000
#define LEDC_RESOLUTION         LEDC_TIMER_10_BIT
#define LEDC_MAX_DUTY           ((1 << LEDC_RESOLUTION) - 1)

#define LEDC_WHITE_GPIO         16
#define LEDC_WHITE_CHANNEL      LEDC_CHANNEL_0
#define LEDC_RED_GPIO           17
#define LEDC_RED_CHANNEL        LEDC_CHANNEL_1


static uint8_t current_brightness = 0;
static uint8_t saved_brightness = 0;
static uint8_t current_mode = 3;

static const char *TAG = "LAMP";

// aplicar brillo y modo
static void apply_led_settings(void) {
    uint32_t duty_value = (current_brightness * LEDC_MAX_DUTY) / 100;
    
    uint32_t white_duty = 0;
    uint32_t red_duty = 0;

    switch (current_mode) {
        case 1: white_duty = duty_value; break; // Solo Blanco
        case 2: red_duty = duty_value; break;   // Solo Rojo
        case 3: // Ambos a la vez
            white_duty = duty_value;
            red_duty = duty_value;
            break;
    }

    ledc_set_duty(LEDC_MODE, LEDC_WHITE_CHANNEL, white_duty);
    ledc_update_duty(LEDC_MODE, LEDC_WHITE_CHANNEL);
    
    ledc_set_duty(LEDC_MODE, LEDC_RED_CHANNEL, red_duty);
    ledc_update_duty(LEDC_MODE, LEDC_RED_CHANNEL);

    printf("L: M:%d B:%d%% (W:%lu R:%lu)\n", 
           current_mode, current_brightness, white_duty, red_duty);
}

void lamp_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_RESOLUTION,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channels[] = {
        // Canal Blanco
        {.gpio_num = LEDC_WHITE_GPIO, .speed_mode = LEDC_MODE, .channel = LEDC_WHITE_CHANNEL, .timer_sel = LEDC_TIMER, .duty = 0},
        // Canal Rojo
        {.gpio_num = LEDC_RED_GPIO, .speed_mode = LEDC_MODE, .channel = LEDC_RED_CHANNEL, .timer_sel = LEDC_TIMER, .duty = 0}
    };

    for (int i = 0; i < 2; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channels[i]));
    }
}

void lamp_set_brightness(uint8_t brightness_percent) {
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    current_brightness = brightness_percent;
    saved_brightness = current_brightness;
    apply_led_settings();
    persistence_save_brightness(brightness_percent);
}

void lamp_set_mode(uint8_t mode) {
    if (mode < 1 || mode > 3) {
        printf("L: Modo %d inválido. Usando modo 3.\n", mode);
        mode = 3;
    }
    current_mode = mode;
    apply_led_settings();
    persistence_save_mode(mode);
}

uint8_t lamp_get_brightness(void) {
    return current_brightness;
}

uint8_t lamp_get_mode(void) {
    return current_mode;
}

void lamp_turn_on(){
    ESP_LOGI(TAG, "Turning on lamp.");
    current_brightness = saved_brightness;
    apply_led_settings();
}

void lamp_turn_off(){
    ESP_LOGI(TAG, "Turning off lamp.");
    saved_brightness = current_brightness;
    current_brightness = 0;
    apply_led_settings();
}
