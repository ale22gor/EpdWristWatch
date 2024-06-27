#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "qrcode.h"


#ifndef WIFIESPWATCH_H
#define WIFIESPWATCH_H


#define EXAMPLE_ESP_MAXIMUM_RETRY 3

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1


static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
bool wifi_init_sta(void);                  
bool wifi_reconnect(void);
void wifi_disconnect(void);
#endif /* WIFIESPWATCH_H */

