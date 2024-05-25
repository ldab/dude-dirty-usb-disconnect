#pragma once

#include <stdio.h>

typedef struct adc_measurement {
  int raw;
  int mv;
} adc_measurement_t;

typedef struct adc {
  adc_measurement_t ch2;
  adc_measurement_t ch3;
} adc_t;

/**
 * @brief Get single ADC measurement in millivolts
 */
void adc_single_shot_mv(adc_t *data);

/**
 * @brief Get single ADC measurement raw
 */
void adc_single_shot_raw(adc_t *data);

/**
 * @brief Initialize ADC, calibrate, etc.
 */
void adc_init(void);