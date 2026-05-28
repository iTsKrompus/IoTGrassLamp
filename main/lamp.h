#ifndef LAMP_H
#define LAMP_H

#include <stdint.h>

// inicialización PWM
void lamp_init(void);

// establece el brillo de las lamparas en un rango de 0 a 100
void lamp_set_brightness(uint8_t brightness_percent);

uint8_t lamp_get_brightness(void);

// cambia el modo: 1 blanco, 2 rojo, 3 rojo blanco
void lamp_set_mode(uint8_t mode);

uint8_t lamp_get_mode(void);

void lamp_turn_on();

void lamp_turn_off();

#endif // LAMP_H