#include "HttpEspWatch.h"

extern const char *TAG;
weather localWeather;


esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
  static char *output_buffer; // Buffer to store response of http request from event handler
  static int output_len;      // Stores number of bytes read
  switch (evt->event_id)
  {
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
    break;
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    /*
     *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
     *  However, event handler can also be used in case chunked encoding is used.
     */
    if (!esp_http_client_is_chunked_response(evt->client))
    {
      // If user_data buffer is configured, copy the response into the buffer
      int copy_len = 0;
      if (evt->user_data)
      {
        // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
        copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
        if (copy_len)
        {
          memcpy(evt->user_data + output_len, evt->data, copy_len);
        }
      }
      else
      {
        int content_len = esp_http_client_get_content_length(evt->client);
        if (output_buffer == NULL)
        {
          // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
          output_buffer = (char *)calloc(content_len + 1, sizeof(char));
          output_len = 0;
          if (output_buffer == NULL)
          {
            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
            return ESP_FAIL;
          }
        }
        copy_len = MIN(evt->data_len, (content_len - output_len));
        if (copy_len)
        {
          memcpy(output_buffer + output_len, evt->data, copy_len);
        }
      }
      output_len += copy_len;
    }

    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    if (output_buffer != NULL)
    {
      // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
      // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
    int mbedtls_err = 0;
    esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
    if (err != 0)
    {
      ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
      ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
    }
    if (output_buffer != NULL)
    {
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  }
  return ESP_OK;
}

void GET_Request()
{

  char *local_response_buffer = (char *)calloc(MAX_HTTP_OUTPUT_BUFFER + 1, sizeof(char));

  if (local_response_buffer == NULL)
  {
    ESP_LOGE(TAG, "Failed to allocate memory for local_response_buffer");
    return;
  }

  esp_http_client_config_t config = {
      .host = "api.openweathermap.org",
      .path = "/data/2.5/forecast",
      .query = "lat=59.73&lon=30.10&units=metric&cnt=7&appid=1f0a2a25433b239334b026c51d09ba76",
      .method = HTTP_METHOD_GET,
      .event_handler = _http_event_handle,
      .user_data = local_response_buffer};

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
     ESP_LOGI(TAG, "Status = %d",
             esp_http_client_get_status_code(client));
  }

  JsonDocument doc;
  JsonDocument filter;
  filter["list"][0]["main"]["temp"] = true;
  filter["list"][0]["weather"][0]["main"] = true;
  filter["city"]["name"] = true;
  filter["city"]["sunrise"] = true;
  filter["city"]["sunset"] = true;

  DeserializationError error = deserializeJson(doc, local_response_buffer, strlen(local_response_buffer), DeserializationOption::Filter(filter));
  // Test if parsing succeeds.
  if (!error)
  {
    // Serial.print(F("deserializeJson() failed: "));
    // erial.println(error.f_str());
    localWeather.cityName = doc["city"]["name"] | "N/A";

    time_t secondsSunrise = doc["city"]["sunrise"] | 0;
    memcpy(&localWeather.sunrise, localtime(&secondsSunrise), sizeof(struct tm));

    time_t secondsSunset = doc["city"]["sunset"] | 0;
    memcpy(&localWeather.sunset, localtime(&secondsSunset), sizeof(struct tm));

    if (!localWeather.weatherDataQueue.empty())
    {
      localWeather.weatherDataQueue = {};
    }

    for (JsonVariant v : doc["list"].as<JsonArray>())
    {
      weatherData tmpWeather;
      tmpWeather.weather = v.as<JsonObject>()["weather"][0]["main"] | "N/A";
      tmpWeather.temp = v.as<JsonObject>()["main"]["temp"];
      localWeather.weatherDataQueue.push(tmpWeather);
    }
  }
  else
  {
    ESP_LOGI(TAG, "deserializeJson() failed: ");
  }

  esp_http_client_cleanup(client);
  free(local_response_buffer);
  local_response_buffer = NULL;
}