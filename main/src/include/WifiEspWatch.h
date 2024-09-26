#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include <stdio.h>
#include <string.h>
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"
#include "qrcode.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"

#ifndef WIFIESPWATCH_H
#define WIFIESPWATCH_H

#define EXAMPLE_ESP_MAXIMUM_RETRY 3

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESPTOUCH_DONE_BIT BIT2

#define PROV_QR_VERSION         "v1"
#define PROV_TRANSPORT_SOFTAP   "softap"
#define PROV_TRANSPORT_BLE      "ble"
#define QRCODE_BASE_URL         "https://espressif.github.io/esp-jumpstart/qrcode.html"

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
void wifi_init_sta(void);
bool wifi_connect(void);
bool wifi_update_prov_and_connect(bool reset);
void wifi_disconnect(void);
void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport);
void get_device_service_name(char *service_name, size_t max);
void wifi_setDefaults(void);

#endif /* WIFIESPWATCH_H */
