#include "freertos/FreeRTOS.h"
#include <arch/cc.h>
#include <IPAddress.h>
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_log.h"

#include "ArduinoJson.h"
#include <sys/param.h>
#include <queue>

#ifndef HTTPESPWATCH_H
#define HTTPESPWATCH_H

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 4096

void GET_Request();
static esp_err_t _http_event_handle(esp_http_client_event_t *evt);

struct weatherData
{
  String weather;
  float temp;
};
struct weather
{
  String cityName;
  tm sunrise;
  tm sunset;
  std::queue<weatherData> weatherDataQueue;
};

#endif /* WIFIESPWATCH_H */