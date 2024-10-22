#include "HttpEspWatch.h"

extern const char *TAG;
weather localWeather;

esp_err_t
_http_event_handle(esp_http_client_event_t *evt)
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

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");

  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("settings", NVS_READONLY, &my_handle);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAG, "Done\n");

    // Read
    ESP_LOGI(TAG, "Reading   lattitude & longtitude from NVS ... ");
    size_t required_size = 6;
    size_t required_summary_size = required_size;

    char lattitude[required_size] = "";
    err = nvs_get_str(my_handle, "lattitude", lattitude, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAG, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Reading   longitude from NVS ... ");
    required_size = 6;
    required_summary_size += required_size;

    char longitude[required_size] = "";
    err = nvs_get_str(my_handle, "longitude", longitude, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAG, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    // Read
    ESP_LOGI(TAG, "Reading  appiid from NVS ... ");
    required_size = 33;
    required_summary_size += required_size;
    char appiid[required_size] = "";
    err = nvs_get_str(my_handle, "appiid", appiid, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAG, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }
    nvs_close(my_handle);
    required_summary_size += 34;
    char query[required_summary_size];
    snprintf(query, required_summary_size, "lat=%s&lon=%s&units=metric&cnt=7&appid=%s", lattitude, longitude, appiid);
    ESP_LOGI(TAG, "Querry: %s\n", query);

    // query[required_summary_size] = '\0';
    esp_http_client_config_t config = {
        .host = "api.openweathermap.org",
        .path = "/data/2.5/forecast",
        .query = query,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 100,
        .event_handler = _http_event_handle,
        .user_data = local_response_buffer};

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
      ESP_LOGI(TAG, "Status = %d",
               esp_http_client_get_status_code(client));
    }

    cJSON *root = cJSON_Parse(local_response_buffer);

    if (root != NULL)
    {
      cJSON *city = cJSON_GetObjectItem(root, "city");

      localWeather.cityName = cJSON_GetObjectItem(city, "name")->valuestring;

      time_t secondsSunrise = cJSON_GetObjectItem(city, "sunrise")->valueint;
      memcpy(&localWeather.sunrise, localtime(&secondsSunrise), sizeof(struct tm));

      time_t secondsSunset = cJSON_GetObjectItem(city, "sunset")->valueint;
      memcpy(&localWeather.sunset, localtime(&secondsSunset), sizeof(struct tm));

      if (!localWeather.weatherDataQueue.empty())
      {
        localWeather.weatherDataQueue = {};
      }

      cJSON *list = cJSON_GetObjectItem(root, "list");
      if (list != NULL)
      {
        for (int i = 0; i < cJSON_GetArraySize(list); i++)
        {
          cJSON *listItem = cJSON_GetArrayItem(list, i);
          if (listItem == NULL)
          {
            break;
          }
          cJSON *main = cJSON_GetObjectItem(listItem, "main");
          cJSON *weather = cJSON_GetObjectItem(listItem, "weather");

          cJSON *temp = cJSON_GetObjectItem(main, "temp");
          cJSON *weatherMain = cJSON_GetObjectItem(weather, "main");
          if (cJSON_IsNumber(temp) && (temp->valuedouble != NULL) && cJSON_IsString(weatherMain) && (weatherMain->valuestring != NULL))
          {
            weatherData tmpWeather;
            tmpWeather.temp = cJSON_GetObjectItem(main, "temp")->valuedouble;
            tmpWeather.weather = cJSON_GetObjectItem(weather, "main")->valuestring;
            // tmpWeather.weather = weatherMain;
            // tmpWeather.temp = temp;
            localWeather.weatherDataQueue.push(tmpWeather);
          }
        }
      }
    }
    else
    {
      ESP_LOGI(TAG, "deserializeJson() failed: ");
    }

    cJSON_Delete(root);
    esp_http_client_cleanup(client);
  }
  free(local_response_buffer);
  local_response_buffer = NULL;
}