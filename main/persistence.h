#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdint.h>
#include "esp_err.h"


esp_err_t persistence_save_mode(uint8_t mode);
esp_err_t persistence_save_brightness(uint8_t brightness);
esp_err_t persistence_save_start_time(int start_time);
esp_err_t persistence_save_end_time(int end_time);


uint8_t persistence_load_mode(void);
uint8_t persistence_load_brightness(void);
int     persistence_load_start_time(void);
int     persistence_load_end_time(void);

#endif // PERSISTENCE_H