#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include <stdio.h>
#include <string.h>

#ifndef WIFIESPWATCH_H
#define WIFIESPWATCH_H

#define EXAMPLE_ESP_MAXIMUM_RETRY 3

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESPTOUCH_DONE_BIT BIT2


void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
void wifi_prov_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data);
void wifi_init_sta(void);
bool wifi_connect(void);
bool wifi_update_prov_and_connect(void);
void wifi_disconnect(void);
#endif /* WIFIESPWATCH_H */
