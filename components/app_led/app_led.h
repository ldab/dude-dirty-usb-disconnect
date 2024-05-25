#pragma once

/**
 * @brief Define blinking type and priority, Zero is highest.
 *
 */
typedef enum {
  BLINK_FAULT = 0,
  BLINK_ON,
  BLINK_IDLE,
  BLINK_MAX,
} led_pattern_t;

/**
 * @brief Start the RGB LED app
 * @note The first state is breathing Yellow, waiting for valid USB state
 */
void app_led_start(void);

/**
 * @brief Set LED state machine to defined patern.
 *
 */
void app_led_state_set(led_pattern_t patern);

/**
 * @brief Clear/Reset LED state machine.
 *
 */
void app_led_state_clear(led_pattern_t patern);
