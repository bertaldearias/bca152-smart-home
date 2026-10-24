
#include "dht11.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <stdint.h>

static portMUX_TYPE dht11_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t wait_for_level(gpio_num_t pin, int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();

    while (gpio_get_level(pin) != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
    }

    return ESP_OK;
}

esp_err_t dht11_read(gpio_num_t pin, int *temperature, int *humidity)
{
    if (!temperature || !humidity) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[5] = {0};

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_direction(pin, GPIO_MODE_INPUT);
    esp_rom_delay_us(30);

    taskENTER_CRITICAL(&dht11_mux);

    esp_err_t err = wait_for_level(pin, 0, 100);
    if (err == ESP_OK) err = wait_for_level(pin, 1, 100);
    if (err == ESP_OK) err = wait_for_level(pin, 0, 100);

    for (int i = 0; i < 40 && err == ESP_OK; i++) {
        err = wait_for_level(pin, 1, 100);
        if (err != ESP_OK) break;

        int64_t start = esp_timer_get_time();
        err = wait_for_level(pin, 0, 100);
        if (err != ESP_OK) break;

        int pulse = (int)(esp_timer_get_time() - start);

        data[i / 8] <<= 1;

        if (pulse > 40) {
            data[i / 8] |= 1;
        }
    }

    taskEXIT_CRITICAL(&dht11_mux);

    if (err != ESP_OK) {
        return err;
    }

    uint8_t checksum =
        (uint8_t)(data[0] + data[1] + data[2] + data[3]);

    if (checksum != data[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    *humidity = data[0];
    *temperature = data[2];

    return ESP_OK;
}
