#include "argtable3/argtable3.h"
#include "cmd_system.h"
// #include "cmd_nvs.h"
// #include "cmd_wifi.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_cdcacm.h"
#include "linenoise/linenoise.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "app_adc.h"
#include "app_hub_control.h"
#include "app_led.h"

static void initialize_nvs(void)
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

void app_main(void)
{
  char *prompt = calloc(strlen(esp_app_get_description()->project_name) + 2, sizeof(char));
  sprintf(prompt, "%s>", esp_app_get_description()->project_name);
  esp_console_repl_t *repl              = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.max_cmdline_length        = 1024;
  repl_config.prompt                    = prompt;
  initialize_nvs();
  adc_init();
  app_led_start();

  /* Register commands */
  esp_console_register_help_command();
  register_system_common();
  // register_nvs();
  // register_wifi();
  register_hub_control();

#if defined(CONFIG_ESP_CONSOLE_USB_CDC)
  esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
  esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error "no console defined"
#endif
  ESP_ERROR_CHECK(esp_console_start_repl(repl));

  free(prompt);
}