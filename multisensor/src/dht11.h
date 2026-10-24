
#ifndef DHT11_H
#define DHT11_H

#include "esp_err.h"
#include "driver/gpio.h"

esp_err_t dht11_read(gpio_num_t pin, int *temperature, int *humidity);

#endif
