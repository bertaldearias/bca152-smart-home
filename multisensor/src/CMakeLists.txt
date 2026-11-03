idf_component_register(
    SRCS "main.c" "dht11.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_timer
        esp_adc
        esp_event
        nvs_flash
        esp_rainmaker
        rmaker_app_network
        rmaker_app_reset
        button
)