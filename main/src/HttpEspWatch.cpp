#include "HttpEspWatch.h"

const char *TAGHTTP = "EPDWatchHTTP";
extern const char *weatherDtKey[];
extern const char *weatherTempKey[];
extern const char *weatherMainKey[];

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
  static char *output_buffer; // Buffer to store response of http request from event handler
  static int output_len;      // Stores number of bytes read
  switch (evt->event_id)
  {
  case HTTP_EVENT_REDIRECT:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_REDIRECT");
    break;
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
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
            ESP_LOGE(TAGHTTP, "Failed to allocate memory for output buffer");
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
    ESP_LOGD(TAGHTTP, "HTTP_EVENT_ON_FINISH");
    if (output_buffer != NULL)
    {
      // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
      // ESP_LOG_BUFFER_HEX(TAGHTTP, output_buffer, output_len);
      free(output_buffer);
      output_buffer = NULL;
    }
    output_len = 0;
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGI(TAGHTTP, "HTTP_EVENT_DISCONNECTED");
    int mbedtls_err = 0;
    esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
    if (err != 0)
    {
      ESP_LOGI(TAGHTTP, "Last esp error code: 0x%x", err);
      ESP_LOGI(TAGHTTP, "Last mbedtls failure: 0x%x", mbedtls_err);
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
    ESP_LOGE(TAGHTTP, "Failed to allocate memory for local_response_buffer");
    return;
  }

  ESP_LOGI(TAGHTTP, "Opening Non-Volatile Storage (NVS) handle... ");

  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("settings", NVS_READONLY, &my_handle);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAGHTTP, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAGHTTP, "Done\n");

    // Read
    ESP_LOGI(TAGHTTP, "Reading   lattitude & longtitude from NVS ... ");
    size_t required_size = 6;
    size_t required_summary_size = required_size;

    char lattitude[required_size] = "";
    err = nvs_get_str(my_handle, "lattitude", lattitude, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAGHTTP, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAGHTTP, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAGHTTP, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    ESP_LOGI(TAGHTTP, "Reading   longitude from NVS ... ");
    required_size = 6;
    required_summary_size += required_size;

    char longitude[required_size] = "";
    err = nvs_get_str(my_handle, "longitude", longitude, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAGHTTP, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAGHTTP, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAGHTTP, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    // Read
    ESP_LOGI(TAGHTTP, "Reading  appiid from NVS ... ");
    required_size = 33;
    required_summary_size += required_size;
    char appiid[required_size] = "";
    err = nvs_get_str(my_handle, "appiid", appiid, &required_size);

    switch (err)
    {
    case ESP_OK:
      ESP_LOGI(TAGHTTP, "Done\n");
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAGHTTP, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAGHTTP, "Error (%s) reading!\n", esp_err_to_name(err));
    }
    nvs_close(my_handle);
    required_summary_size += 34;
    char query[required_summary_size];
    snprintf(query, required_summary_size, "lat=%s&lon=%s&units=metric&appid=%s", lattitude, longitude, appiid);
    ESP_LOGI(TAGHTTP, "Querry: %s\n", query);

    // query[required_summary_size] = '\0';
    esp_http_client_config_t config = {
        .host = "api.openweathermap.org",
        .path = "/data/2.5/forecast",
        .query = query,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .event_handler = _http_event_handle,
        .user_data = local_response_buffer};

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
      ESP_LOGI(TAGHTTP, "Status = %d",
               esp_http_client_get_status_code(client));
    }

    cJSON *root = cJSON_Parse(local_response_buffer);

    if (root != NULL)
    {
      cJSON *city = cJSON_GetObjectItem(root, "city");

      ESP_LOGI(TAGHTTP, "Opening Non-Volatile Storage (NVS) handle... ");
      nvs_handle_t location_handle;
      esp_err_t errNvsLocation = nvs_open("location", NVS_READWRITE, &location_handle);

      if (errNvsLocation != ESP_OK)
      {
        ESP_LOGE(TAGHTTP, "Error (%s) opening NVS location handle!\n", esp_err_to_name(errNvsLocation));
      }
      else
      {

        ESP_LOGI(TAGHTTP, "Done\n");

        // Write
        esp_err_t err = nvs_set_str(location_handle, "city", cJSON_GetObjectItem(city, "name")->valuestring);
        // ESP_LOGI(TAGHTTP, "City %s\n",cJSON_GetObjectItem(city, "name")->valuestring);
        // ESP_LOGI(TAGHTTP, "sunrise %d\n",cJSON_GetObjectItem(city, "sunrise")->valueint);
        // ESP_LOGI(TAGHTTP, "sunset %d\n",cJSON_GetObjectItem(city, "sunset")->valueint);

        int64_t sunrise = cJSON_GetObjectItem(city, "sunrise")->valueint;
        err = nvs_set_i64(location_handle, "sunrise", sunrise);
        int64_t sunset = cJSON_GetObjectItem(city, "sunset")->valueint;
        err = nvs_set_i64(location_handle, "sunset", sunset);
      }

      ESP_LOGI(TAGHTTP, "Committing updates in NVS ... ");

      errNvsLocation = nvs_commit(location_handle);
      // Close
      nvs_close(location_handle);

      nvs_handle_t weather_handle;
      esp_err_t errNvsWeather = nvs_open("weather", NVS_READWRITE, &weather_handle);
      if (errNvsWeather != ESP_OK)
      {
        ESP_LOGE(TAGHTTP, "Error (%s) opening NVS weather handle!\n", esp_err_to_name(errNvsWeather));
      }
      else
      {
        cJSON *list = cJSON_GetObjectItem(root, "list");
        if (list != NULL)
        {
          for (int i = 0; i < cJSON_GetArraySize(list) && i < WEATHERTIMESTUMPS; i++)
          {
            cJSON *listItem = cJSON_GetArrayItem(list, i);

            cJSON *main = cJSON_GetObjectItem(listItem, "main");
            cJSON *weather = cJSON_GetObjectItem(listItem, "weather");
            cJSON *weatherItem = cJSON_GetArrayItem(weather, 0);

            cJSON *dt = cJSON_GetObjectItem(listItem, "dt");
            cJSON *temp = cJSON_GetObjectItem(main, "temp");
            cJSON *weatherId = cJSON_GetObjectItem(weatherItem, "id");
            esp_err_t err;
            if (true)
            {
              ESP_LOGI(TAGHTTP, "weather: %d\n", weatherId->valueint);
              ESP_LOGI(TAGHTTP, "temp: %d\n", (int)temp->valuedouble);
              ESP_LOGI(TAGHTTP, "dt: %d\n", dt->valueint);

              int8_t tempInt = (int)temp->valuedouble;
              err = nvs_set_i8(weather_handle, weatherTempKey[i], tempInt);

              u16_t weatherIdInt = weatherId->valueint;
              err = nvs_set_u16(weather_handle, weatherMainKey[i], weatherIdInt);

              int64_t dtInt = dt->valueint;
              err = nvs_set_i64(weather_handle, weatherDtKey[i], dtInt);
            }
          }
        }
      }
      // Commit written value.
      ESP_LOGI(TAGHTTP, "Committing updates in NVS ... ");
      errNvsWeather = nvs_commit(weather_handle);
      // Close
      nvs_close(weather_handle);
    }
    else
    {
      ESP_LOGI(TAGHTTP, "deserializeJson() failed: ");
    }

    cJSON_Delete(root);
    esp_http_client_cleanup(client);
  }
  free(local_response_buffer);
  local_response_buffer = NULL;
}