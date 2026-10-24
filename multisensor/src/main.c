
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "dht11.h"

#define DHT11_PIN GPIO_NUM_4
#define LDR_ADC_CHANNEL ADC_CHANNEL_4  // GPIO5 on ESP32-S3

static const char *TAG = "MULTISENSOR";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting DHT11 + LDR sensor test...");

    // Initialize ADC1
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        adc_handle,
        LDR_ADC_CHANNEL,
        &channel_config
    ));

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1)
    {
        int temperature = 0;
        int humidity = 0;
        int light_raw = 0;

        // Read DHT11
        esp_err_t dht_result = dht11_read(
            DHT11_PIN,
            &temperature,
            &humidity
        );

        if (dht_result == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %d C", temperature);
            ESP_LOGI(TAG, "Humidity: %d %%", humidity);
        } else {
            ESP_LOGE(TAG, "DHT11 read failed: %s",
                     esp_err_to_name(dht_result));
        }

        // Read LDR
        esp_err_t ldr_result = adc_oneshot_read(
            adc_handle,
            LDR_ADC_CHANNEL,
            &light_raw
        );

        if (ldr_result == ESP_OK) {
            ESP_LOGI(TAG, "Light Level (ADC): %d", light_raw);
        } else {
            ESP_LOGE(TAG, "LDR read failed: %s",
                     esp_err_to_name(ldr_result));
        }

        ESP_LOGI(TAG, "----------------------------");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
