#include "app_adc.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

const static char *TAG                          = "adc";

static adc_cali_handle_t adc1_cali_chan2_handle = NULL;
static adc_cali_handle_t adc1_cali_chan3_handle = NULL;
static adc_oneshot_unit_handle_t adc1_handle    = NULL;

static void adc_single_shot(adc_t *data, bool raw)
{
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &data->ch2.raw));
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &data->ch3.raw));
  ESP_LOGI(TAG, "ADC1 Channel[%d] Raw Data: %d", ADC_CHANNEL_2, data->ch2.raw);
  ESP_LOGI(TAG, "ADC1 Channel[%d] Raw Data: %d", ADC_CHANNEL_3, data->ch3.raw);

  if (!raw) {
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan2_handle, data->ch2.raw, &data->ch2.mv));
    ESP_LOGI(TAG, "ADC1 Channel[%d] Cali Voltage: %d mV", ADC_CHANNEL_2, data->ch2.mv);

    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan2_handle, data->ch3.raw, &data->ch3.mv));
    ESP_LOGI(TAG, "ADC1 Channel[%d] Cali Voltage: %d mV", ADC_CHANNEL_3, data->ch3.mv);
  }
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
  adc_cali_handle_t handle = NULL;
  esp_err_t ret            = ESP_FAIL;
  bool calibrated          = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

  *out_handle = handle;
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Calibration Success");
  } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
    ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
  } else {
    ESP_LOGE(TAG, "Invalid arg or no memory");
  }

  return calibrated;
}

void adc_init(void)
{
  //-------------ADC1 Init---------------//
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = ADC_UNIT_1,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  //-------------ADC1 Config---------------//
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_12,
      .atten    = ADC_ATTEN_DB_12,
  };

  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &config));
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));

  //-------------ADC1 Calibration Init---------------//
  adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_chan2_handle);
  adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_chan3_handle);
}

void adc_single_shot_mv(adc_t *data)
{
  adc_single_shot(data, false); //
}

void adc_single_shot_raw(adc_t *data)
{
  adc_single_shot(data, true); //
}
