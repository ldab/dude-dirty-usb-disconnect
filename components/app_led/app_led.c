#include "app_led.h"

#include "cmd_led_indicator.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_indicator.h"
#include <inttypes.h>
#include <stdio.h>

#define GPIO_LED_RED_PIN       5
#define GPIO_LED_GREEN_PIN     2
#define GPIO_LED_BLUE_PIN      8
#define GPIO_ACTIVE_LEVEL      0
#define LEDC_LED_RED_CHANNEL   0
#define LEDC_LED_GREEN_CHANNEL 1
#define LEDC_LED_BLUE_CHANNEL  2

static const char *TAG                       = "app_led";
static led_indicator_handle_t led_handle     = NULL;

static const blink_step_t double_red_blink[] = {
    {LED_BLINK_RGB, SET_RGB(255, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 1000},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1000},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t breath_green_slow_blink[] = {
    {LED_BLINK_HSV, SET_HSV(108, 255, 127), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 1500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1500},
    {LED_BLINK_LOOP, 0, 0},
};

static const blink_step_t breath_blue_slow_blink[] = {
    {LED_BLINK_HSV, SET_HSV(240, 255, 127), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 1500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 1500},
    {LED_BLINK_LOOP, 0, 0},
};

blink_step_t const *led_mode[] = {
    [BLINK_FAULT] = double_red_blink,
    [BLINK_ON]    = breath_green_slow_blink,
    [BLINK_IDLE]  = breath_blue_slow_blink,
    [BLINK_MAX]   = NULL,
};

void app_led_start(void)
{
  led_indicator_rgb_config_t rgb_config = {
      .is_active_level_high = GPIO_ACTIVE_LEVEL,
      .timer_inited         = false,
      .timer_num            = LEDC_TIMER_0,
      .red_gpio_num         = GPIO_LED_RED_PIN,
      .green_gpio_num       = GPIO_LED_GREEN_PIN,
      .blue_gpio_num        = GPIO_LED_BLUE_PIN,
      .red_channel          = LEDC_LED_RED_CHANNEL,
      .green_channel        = LEDC_LED_GREEN_CHANNEL,
      .blue_channel         = LEDC_LED_BLUE_CHANNEL,
  };

  const led_indicator_config_t config = {
      .mode                     = LED_RGB_MODE,
      .led_indicator_rgb_config = &rgb_config,
      .blink_lists              = led_mode,
      .blink_list_num           = BLINK_MAX,
  };

  led_handle = led_indicator_create(&config);
  assert(led_handle != NULL);

  ESP_LOGI(TAG, "start app_led");

  led_indicator_start(led_handle, BLINK_IDLE);
}

void app_led_state_set(led_pattern_t patern)
{
  led_indicator_start(led_handle, patern); //
}

void app_led_state_clear(led_pattern_t patern)
{
  led_indicator_stop(led_handle, patern); //
}
