#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

#ifndef BATTERYESPWATCH_H
#define BATTERYESPWATCH_H

void powerInit();
int powerMeasure();
void stopPowerMeasure();
bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);


#endif /* BATTERYESPWATCH_H */