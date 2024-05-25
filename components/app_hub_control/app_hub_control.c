#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "assert.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "app_adc.h"
#include "app_hub_control.h"
#include "app_led.h"

#define HUB_RST   6
#define HUB_EN    17
#define HUB_FAULT 18
#define ON        1
#define OFF       0

#ifndef LOG_HUB_CONTROL_LEVEL
#define LOG_HUB_CONTROL_LEVEL ESP_LOG_WARN
#endif

// 1.5A capable source will pull CC up with:
// * 22kOhms@5V
// * 12KOhms@3.3V
// Table 4-36 says min 700mV for 1.5A
#define CC_1_5AMPS_CAPABLE_MV 700

typedef struct hub_setting {
  char *key;
  uint8_t val;
} hub_setting_t;

hub_setting_t hub_settings[] = {
    {
        .key = "override_cc",
        .val = false,
    },
    {
        .key = "auto_on",
        .val = true,
    },
};

static struct {
  struct arg_str *setting;
  struct arg_int *enable;
  struct arg_end *end;
} settings_args;

static const char *TAG                    = "app_hub_control";
static EventGroupHandle_t hub_event_group = NULL;
const static int FAULT_BIT                = BIT0;
const static int ON_CMD_BIT               = BIT1;
const static int OFF_CMD_BIT              = BIT2;
const static int STATUS_BIT               = BIT3;

static void save_config(hub_setting_t hub_setting);

static int hub_restart(int argc, char **argv)
{
  printf("Restarting USB HUB...\n");
  gpio_set_level(HUB_RST, OFF);
  vTaskDelay(pdMS_TO_TICKS(1000));
  gpio_set_level(HUB_RST, ON);

  return 0;
}

static int ports_off(int argc, char **argv)
{
  xEventGroupSetBits(hub_event_group, OFF_CMD_BIT);
  return 0;
}

static int ports_on(int argc, char **argv)
{
  xEventGroupSetBits(hub_event_group, ON_CMD_BIT);
  return 0;
}

static int port_restart(int argc, char **argv)
{
  printf("Restarting USB Ports...\n");
  ports_off(0, NULL);
  vTaskDelay(pdMS_TO_TICKS(1000));
  ports_on(0, NULL);

  return 0;
}

static int hub_status(int argc, char **argv)
{
  xEventGroupSetBits(hub_event_group, STATUS_BIT);
  return 0;
}

static int hub_settings_cmd(int argc, char **argv)
{
  int nerrors = arg_parse(argc, argv, (void **)&settings_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, settings_args.end, argv[0]);
    return 1;
  }

  assert(settings_args.setting->count == 1);
  assert(settings_args.enable->count == 1);

  const hub_setting_t hub_setting = {
      .key = settings_args.setting->sval[0],
      .val = settings_args.enable->ival[0],
  };

  bool settings_exist = false;
  for (size_t i = 0; i < sizeof(hub_settings) / sizeof(hub_setting_t); i++) {
    if (strcmp(hub_setting.key, hub_settings[i].key) == 0) {
      settings_exist = true;
      break;
    }
  }

  if (!settings_exist) {
    printf("Setting %s is not valid!\n", hub_setting.key);
    return 1;
  }

  save_config(hub_setting);

  return 0;
}

static void register_hub_cmds(void)
{
  esp_console_cmd_t status_cmd = {
      .command = "hub_status",
      .help    = "HUB Controller Status",
      .hint    = NULL,
      .func    = &hub_status,
  };

  const esp_console_cmd_t restart_cmd = {
      .command = "hub_restart",
      .help    = "HUB Controller restart",
      .hint    = NULL,
      .func    = &hub_restart,
  };

  const esp_console_cmd_t ports_off_cmd = {
      .command = "hub_ports_off",
      .help    = "Turn USB Ports Off",
      .hint    = NULL,
      .func    = &ports_off,
  };

  const esp_console_cmd_t ports_on_cmd = {
      .command = "hub_ports_on",
      .help    = "Turn USB Ports On",
      .hint    = NULL,
      .func    = &ports_on,
  };

  const esp_console_cmd_t port_restart_cmd = {
      .command = "hub_port_restart",
      .help    = "HUB ports restart",
      .hint    = NULL,
      .func    = &port_restart,
  };

  uint32_t settings_options_size = 0;
  for (size_t i = 0; i < sizeof(hub_settings) / sizeof(hub_setting_t); i++) {
    settings_options_size += strlen(hub_settings[i].key) + 1; // add "|"
  }
  settings_options_size += 2; // null terminated + "<"
  char *settings_options = (char *)calloc(settings_options_size, sizeof(char));
  assert(settings_options != NULL);

  settings_options[0] = '<';
  for (size_t i = 0; i < sizeof(hub_settings) / sizeof(hub_setting_t); i++) {
    strcat(settings_options, hub_settings[i].key);
    strcat(settings_options, (i < (sizeof(hub_settings) / sizeof(hub_setting_t) - 1)) ? "|" : ">"); // last char
  }

  settings_args.setting                = arg_str1(NULL, NULL, (const char *)settings_options, "HUB setting");
  settings_args.enable                 = arg_int1(NULL, NULL, "<0|1>", "Enable (1) or disable (0) setting");
  settings_args.end                    = arg_end(2);
  const esp_console_cmd_t settings_cmd = {
      .command  = "hub_settings",
      .help     = "Set HUB settings, applied after restart!",
      .hint     = NULL,
      .func     = &hub_settings_cmd,
      .argtable = &settings_args,
  };

  ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&restart_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&ports_off_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&ports_on_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&port_restart_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&settings_cmd));

  free(settings_options);
}

static void load_setting(hub_setting_t *hub_setting)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("hub_settings", NVS_READWRITE, &nvs_handle);
  ESP_ERROR_CHECK(err);

  err = nvs_get_u8(nvs_handle, hub_setting->key, &hub_setting->val);
  ESP_ERROR_CHECK(err);

  nvs_close(nvs_handle);
  ESP_ERROR_CHECK(err);
}

static void load_config()
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("hub_settings", NVS_READWRITE, &nvs_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  }

  ESP_ERROR_CHECK(err);

  for (size_t i = 0; i < sizeof(hub_settings) / sizeof(hub_setting_t); i++) {
    err = nvs_get_u8(nvs_handle, hub_settings[i].key, &hub_settings[i].val);
    switch (err) {
    case ESP_OK:
      ESP_LOGD(TAG, "Loaded %s setting: %d", hub_settings[i].key, hub_settings[i].val);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGD(TAG, "NVS Settings is not initialized yet, do it now");
      err = nvs_set_u8(nvs_handle, hub_settings[i].key, hub_settings[i].val);
      ESP_ERROR_CHECK(err);
      err = nvs_commit(nvs_handle);
      ESP_ERROR_CHECK(err);
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
  }

  nvs_close(nvs_handle);
  ESP_ERROR_CHECK(err);
}

static void save_config(hub_setting_t hub_setting)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("hub_settings", NVS_READWRITE, &nvs_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  }

  ESP_ERROR_CHECK(err);

  err = nvs_set_u8(nvs_handle, hub_setting.key, hub_setting.val);
  ESP_ERROR_CHECK(err);
  err = nvs_commit(nvs_handle);
  ESP_ERROR_CHECK(err);
  nvs_close(nvs_handle);
  ESP_ERROR_CHECK(err);
}

static bool is_faulty()
{
  return !gpio_get_level(HUB_FAULT); // active low
}

static void handle_fault_bit()
{
  static bool state_backup;

  if (is_faulty()) {
    state_backup = gpio_get_level(HUB_EN);
    app_led_state_set(BLINK_FAULT);
    ports_off(0x00, NULL);
  } else {
    app_led_state_clear(BLINK_FAULT);
    if (state_backup) {
      ports_on(0x00, NULL);
    }
  }
}

static void handle_off_cmd()
{
  printf("USB Ports OFF...\n");
  gpio_set_level(HUB_EN, OFF);
  app_led_state_clear(BLINK_ON);
}

static void handle_on_cmd()
{
  if (is_faulty()) {
    printf("HUB Fault Pin is set, do nothing...\n");
    return;
  }

  printf("USB Ports ON...\n");
  gpio_set_level(HUB_EN, ON);
  app_led_state_set(BLINK_ON);
}

static void hub_control_task(void *arg)
{
  adc_t adc;

  load_config();

  // Check if Host can supply 1.5A
  hub_setting_t override_cc = {.key = "override_cc"};
  load_setting(&override_cc);
  do {
    adc_single_shot_mv(&adc);
    ESP_LOGD(TAG, "adc.ch2.mv %d adc.ch3.mv %d", adc.ch2.mv, adc.ch3.mv);
  } while ((adc.ch2.mv < CC_1_5AMPS_CAPABLE_MV && adc.ch3.mv < CC_1_5AMPS_CAPABLE_MV) && !override_cc.val);

  // If we get here, either CC is valid or overwritten, enable TPS
  hub_setting_t auto_on = {.key = "auto_on"};
  load_setting(&auto_on);
  if (auto_on.val) {
    handle_on_cmd();
  }

  while (1) {
    xEventGroupWaitBits(hub_event_group, 0x00FFFFFF, pdFALSE, pdFALSE, portMAX_DELAY);
    EventBits_t event_bits = xEventGroupClearBits(hub_event_group, 0x00FFFFFF);

    ESP_LOGD(TAG, "event_bits %lu", event_bits);

    if (event_bits & FAULT_BIT) {
      handle_fault_bit();
    }
    if (event_bits & ON_CMD_BIT) {
      handle_on_cmd();
    }
    if (event_bits & OFF_CMD_BIT) {
      handle_off_cmd();
    }
    if (event_bits & STATUS_BIT) {
      printf("FW version: %s\n", esp_app_get_description()->version);
      printf("Hub is: %s\n", gpio_get_level(HUB_EN) ? "Enabled" : "Disabled");
      printf("Fault: %s\n", is_faulty() ? "Overcurrent or Overtemperature" : "None");
      if (adc.ch2.mv < CC_1_5AMPS_CAPABLE_MV && adc.ch3.mv < CC_1_5AMPS_CAPABLE_MV) {
        printf("USB Host does not provide 1.5A\n");
      } else {
        printf("USB Host current is adequate\n");
      }
    }
  }

  vTaskDelete(NULL);
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xEventGroupSetBitsFromISR(hub_event_group, FAULT_BIT, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void gpio_init()
{
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};

  io_conf.pin_bit_mask  = (1ULL << HUB_EN);
  io_conf.intr_type     = GPIO_INTR_DISABLE;
  io_conf.mode          = GPIO_MODE_INPUT_OUTPUT;
  io_conf.pull_down_en  = 0;
  io_conf.pull_up_en    = 0;
  gpio_config(&io_conf);

  io_conf.pin_bit_mask = (1ULL << HUB_RST);
  io_conf.intr_type    = GPIO_INTR_DISABLE;
  io_conf.mode         = GPIO_MODE_OUTPUT_OD;
  io_conf.pull_down_en = 0;
  io_conf.pull_up_en   = 1;
  gpio_config(&io_conf);

  // interrupt of falling edge
  io_conf.pin_bit_mask = (1ULL << HUB_FAULT);
  io_conf.mode         = GPIO_MODE_INPUT;
  io_conf.intr_type    = GPIO_INTR_ANYEDGE;
  io_conf.pull_up_en   = 1;
  io_conf.pull_down_en = 0;
  gpio_config(&io_conf);

  // install gpio isr service
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  // hook isr handler for specific gpio pin
  gpio_isr_handler_add(HUB_FAULT, gpio_isr_handler, (void *)HUB_FAULT);

  gpio_set_level(HUB_RST, ON);
}

void register_hub_control(void)
{
  gpio_init();

  esp_log_level_set(TAG, LOG_HUB_CONTROL_LEVEL);

  register_hub_cmds();

  hub_event_group = xEventGroupCreate();
  assert(hub_event_group);
  assert(xTaskCreate(hub_control_task, "hub_control_task", 4096, NULL, 0, NULL));

  ESP_LOGI(TAG, "register_hub_control");
}