
#include <stdio.h>
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_adc/adc_oneshot.h>

#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_ota.h>

#include <app_network.h>
#include "dht11.h"

/* HARDWARE CONFIGURATION */

#define DHT11_PIN GPIO_NUM_4
#define LDR_ADC_CHANNEL ADC_CHANNEL_4  // GPIO5 on ESP32-S3

#define WIFI_RESET_GPIO GPIO_NUM_0
#define WIFI_RESET_BUTTON_TIMEOUT 3
#define FACTORY_RESET_BUTTON_TIMEOUT 10

/* SENSOR AND CLOUD REPORTING INTERVALS */

#define SENSOR_READ_INTERVAL_MS 5000
#define CLOUD_REPORT_INTERVAL_MS 30000

static const char *TAG = "MULTISENSOR";

static esp_rmaker_param_t *temperature_param;
static esp_rmaker_param_t *humidity_param;
static esp_rmaker_param_t *light_param;

static adc_oneshot_unit_handle_t adc_handle;

/* BOOT BUTTON INITIALIZATION */

static void init_wifi_reset_button(void)
{
    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };

    button_gpio_config_t gpio_cfg = {
        .gpio_num = WIFI_RESET_GPIO,
        .active_level = 0,
        .enable_power_save = false,
    };

    button_handle_t btn_handle = NULL;

    esp_err_t err = iot_button_new_gpio_device(
        &btn_cfg,
        &gpio_cfg,
        &btn_handle
    );

    if (err != ESP_OK || btn_handle == NULL) {
        ESP_LOGE(
            TAG,
            "Failed to initialize BOOT reset button: %s",
            esp_err_to_name(err)
        );
        return;
    }

    app_reset_button_register(
        btn_handle,
        WIFI_RESET_BUTTON_TIMEOUT,
        FACTORY_RESET_BUTTON_TIMEOUT
    );

    ESP_LOGI(
        TAG,
        "BOOT button registered: 3s Wi-Fi reset, 10s factory reset"
    );
}

/* SENSOR TASK */

static void sensor_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));

    int last_reported_temperature = 0;
    int last_reported_humidity = 0;
    int last_reported_light = 0;

    bool temperature_reported = false;
    bool humidity_reported = false;
    bool light_reported = false;

    TickType_t last_report_time =
        xTaskGetTickCount() -
        pdMS_TO_TICKS(CLOUD_REPORT_INTERVAL_MS);

    while (1) {
        int temperature = 0;
        int humidity = 0;
        int light_raw = 0;

        bool dht_valid = false;
        bool light_valid = false;

        /* READ DHT11 */

        esp_err_t result = dht11_read(
            DHT11_PIN,
            &temperature,
            &humidity
        );

        if (result == ESP_OK) {
            dht_valid = true;

            ESP_LOGI(
                TAG,
                "Temperature: %d C",
                temperature
            );

            ESP_LOGI(
                TAG,
                "Humidity: %d %%",
                humidity
            );
        } else {
            ESP_LOGE(
                TAG,
                "DHT11 failed: %s",
                esp_err_to_name(result)
            );
        }

        /* READ LDR */

        esp_err_t adc_result = adc_oneshot_read(
            adc_handle,
            LDR_ADC_CHANNEL,
            &light_raw
        );

        if (adc_result == ESP_OK) {
            light_valid = true;

            ESP_LOGI(
                TAG,
                "Light Level (ADC): %d",
                light_raw
            );
        } else {
            ESP_LOGE(
                TAG,
                "LDR ADC failed: %s",
                esp_err_to_name(adc_result)
            );
        }

        /* CHECK CLOUD REPORT INTERVAL */

        TickType_t now = xTaskGetTickCount();

        if ((now - last_report_time) >=
            pdMS_TO_TICKS(CLOUD_REPORT_INTERVAL_MS)) {

            ESP_LOGI(
                TAG,
                "Checking sensor values for cloud reporting..."
            );

            /* REPORT TEMPERATURE IF CHANGED */

            if (dht_valid &&
                (!temperature_reported ||
                 temperature != last_reported_temperature)) {

                esp_err_t report_result =
                    esp_rmaker_param_update_and_report(
                        temperature_param,
                        esp_rmaker_float((float)temperature)
                    );

                if (report_result == ESP_OK) {
                    last_reported_temperature = temperature;
                    temperature_reported = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Temperature report failed: %s",
                        esp_err_to_name(report_result)
                    );
                }
            }

            /* REPORT HUMIDITY IF CHANGED */

            if (dht_valid &&
                (!humidity_reported ||
                 humidity != last_reported_humidity)) {

                esp_err_t report_result =
                    esp_rmaker_param_update_and_report(
                        humidity_param,
                        esp_rmaker_float((float)humidity)
                    );

                if (report_result == ESP_OK) {
                    last_reported_humidity = humidity;
                    humidity_reported = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Humidity report failed: %s",
                        esp_err_to_name(report_result)
                    );
                }
            }

            /* REPORT LIGHT IF CHANGED */

            if (light_valid &&
                (!light_reported ||
                 light_raw != last_reported_light)) {

                esp_err_t report_result =
                    esp_rmaker_param_update_and_report(
                        light_param,
                        esp_rmaker_int(light_raw)
                    );

                if (report_result == ESP_OK) {
                    last_reported_light = light_raw;
                    light_reported = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "Light report failed: %s",
                        esp_err_to_name(report_result)
                    );
                }
            }

            last_report_time = now;
        }

        /* READ SENSORS EVERY 5 SECONDS */

        vTaskDelay(
            pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS)
        );
    }
}

/* MAIN APPLICATION */

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "Starting ESP32-S3 RainMaker Multisensor"
    );

    /* NVS INITIALIZATION */

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    /* NETWORK INITIALIZATION */

    app_network_init();

    /* INITIALIZE BOOT BUTTON */

    init_wifi_reset_button();

    /* ADC INITIALIZATION */

    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &adc_config,
            &adc_handle
        )
    );

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            LDR_ADC_CHANNEL,
            &channel_config
        )
    );

    /* RAINMAKER INITIALIZATION */

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false
    };

    esp_rmaker_node_t *node = esp_rmaker_node_init(
        &rainmaker_cfg,
        "ESP32-S3 Multisensor",
        "Multisensor"
    );

    if (!node) {
        ESP_LOGE(
            TAG,
            "RainMaker node initialization failed"
        );
        return;
    }

    /* CREATE MULTISENSOR DEVICE */

    esp_rmaker_device_t *device =
        esp_rmaker_device_create(
            "Multisensor",
            NULL,
            NULL
        );

    if (!device) {
        ESP_LOGE(
            TAG,
            "Device creation failed"
        );
        return;
    }

    /* TEMPERATURE PARAMETER */

    temperature_param = esp_rmaker_param_create(
        "Temperature",
        ESP_RMAKER_PARAM_TEMPERATURE,
        esp_rmaker_float(0),
        PROP_FLAG_READ
    );

    /* HUMIDITY PARAMETER */

    humidity_param = esp_rmaker_param_create(
        "Humidity",
        "esp.param.humidity",
        esp_rmaker_float(0),
        PROP_FLAG_READ
    );

    /* LIGHT LEVEL PARAMETER */

    light_param = esp_rmaker_param_create(
        "Light Level (ADC)",
        NULL,
        esp_rmaker_int(0),
        PROP_FLAG_READ
    );

    /* ADD PARAMETERS */

    esp_rmaker_device_add_param(
        device,
        temperature_param
    );

    esp_rmaker_device_add_param(
        device,
        humidity_param
    );

    esp_rmaker_device_add_param(
        device,
        light_param
    );

    esp_rmaker_device_assign_primary_param(
        device,
        temperature_param
    );

    esp_rmaker_node_add_device(
        node,
        device
    );

    /* START RAINMAKER */

    esp_rmaker_ota_enable_default();

    ESP_ERROR_CHECK(
        esp_rmaker_start()
    );

    /* START NETWORK */

    err = app_network_start(POP_TYPE_RANDOM);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Network start failed: %s",
            esp_err_to_name(err)
        );
        return;
    }

    /* START SENSOR TASK */

    xTaskCreate(
        sensor_task,
        "sensor_task",
        4096,
        NULL,
        5,
        NULL
    );
}
