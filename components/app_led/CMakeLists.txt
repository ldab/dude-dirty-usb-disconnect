idf_component_register(SRCS "app_led.c"
                       INCLUDE_DIRS "."
                       REQUIRES cmd_led_indicator led_indicator)
