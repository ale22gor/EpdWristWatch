#include "BatteryEspWatch.h"

static adc_oneshot_unit_handle_t adc1_handle;


void powerInit()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));
}

int powerMeasure()
{
    int voltage;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &voltage));
    return voltage;
}

void stopPowerMeasure()
{
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}
