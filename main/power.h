// power.h
#ifndef POWER_H
#define POWER_H

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t power_init(i2c_master_bus_handle_t bus_handle);


float power_get_watts(void);

#endif // POWER_H