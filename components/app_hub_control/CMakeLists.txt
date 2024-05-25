idf_component_register(SRCS "app_hub_control.c"
                       INCLUDE_DIRS "."
                       REQUIRES console driver app_adc app_led nvs_flash esp_app_format)
