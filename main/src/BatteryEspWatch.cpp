#include "BatteryEspWatch.h"

const char *TAGBATTERY = "EPDWatchBATTERY";


static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;


void powerInit()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_9,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));
    //adc_calibration_init(ADC_UNIT_1, ADC_CHANNEL_6, ADC_ATTEN_DB_12, &adc1_cali_handle);

}

int powerMeasure()
{
    int raw;
    int voltage;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &raw));
    //adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage);
    int percentBattery = (raw * 100 / 512);

    return percentBattery;
}

void stopPowerMeasure()
{
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}

bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAGBATTERY, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAGBATTERY, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAGBATTERY, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAGBATTERY, "Invalid arg or no memory");
    }

    return calibrated;
}
