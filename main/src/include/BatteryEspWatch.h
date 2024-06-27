#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#ifndef BATTERYESPWATCH_H
#define BATTERYESPWATCH_H

void powerInit();
int powerMeasure();
void stopPowerMeasure();

#endif /* BATTERYESPWATCH_H */