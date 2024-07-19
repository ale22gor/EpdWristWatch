#include "WifiEspWatch.h"

static int s_retry_num = 0;

extern const char *TAG;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
      {
        esp_wifi_connect();
        s_retry_num++;
        ESP_LOGI(TAG, "retry to connect to the AP");
      }
      else
      {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
      ESP_LOGI(TAG, "connect to the AP fail");
      break;
    default:
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_prov_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
    {
      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED:
    {
      esp_wifi_connect();
      xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
      break;
    }
    default:
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
  else if (event_base == SC_EVENT)
  {
    switch (event_id)
    {
    case SC_EVENT_SCAN_DONE:
    {
      ESP_LOGI(TAG, "Scan done");
    }
    break;

    case SC_EVENT_FOUND_CHANNEL:
    {
      ESP_LOGI(TAG, "Found channel");
    }
    break;

    case SC_EVENT_GOT_SSID_PSWD:
    {
      ESP_LOGI(TAG, "Got SSID and password");

      smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
      wifi_config_t wifi_config;
      uint8_t ssid[33] = {0};
      uint8_t password[65] = {0};
      uint8_t rvd_data[33] = {0};

      bzero(&wifi_config, sizeof(wifi_config_t));
      memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
      memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

      memcpy(ssid, evt->ssid, sizeof(evt->ssid));
      memcpy(password, evt->password, sizeof(evt->password));
      ESP_LOGI(TAG, "SSID:%s", ssid);
      ESP_LOGI(TAG, "PASSWORD:%s", password);
      if (evt->type == SC_TYPE_ESPTOUCH_V2)
      {
        ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
        ESP_LOGI(TAG, "RVD_DATA:");
        for (int i = 0; i < 33; i++)
        {
          printf("%02x ", rvd_data[i]);
        }
        printf("\n");
      }

      ESP_ERROR_CHECK(esp_wifi_disconnect());
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      esp_wifi_connect();
    }
    break;
    case SC_EVENT_SEND_ACK_DONE:
    {
      xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
    break;
    default:
    }
    break;
  }
}

void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_LOGI(TAG, "WIFI init finished.");
}

void wifi_disconnect(void)
{
  // vEventGroupDelete(s_wifi_event_group);
  ESP_ERROR_CHECK(esp_wifi_stop());
}

bool wifi_update_prov_and_connect(void)
{
  bool wifiStatus = false;
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  esp_event_handler_instance_t instance_any_sc_id;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_prov_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_prov_event_handler,
                                                      NULL,
                                                      &instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(SC_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_prov_event_handler,
                                                      NULL,
                                                      &instance_any_sc_id));

  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Provision start.");

  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         ESPTOUCH_DONE_BIT,
                                         pdTRUE,
                                         pdFALSE,
                                         portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & ESPTOUCH_DONE_BIT)
  {
    ESP_LOGI(TAG, "smartconfig over");
    wifiStatus = true;
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  /* The event will not be processed after unregister */
  esp_smartconfig_stop();
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(SC_EVENT, ESP_EVENT_ANY_ID, instance_any_sc_id));
  return wifiStatus;
}

bool wifi_connect(void)
{
  bool wifiStatus = false;
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &wifi_event_handler,
                                                      NULL,
                                                      &instance_got_ip));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdTRUE,
                                         pdFALSE,
                                         portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "connected to ap");
    wifiStatus = true;
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGI(TAG, "Failed to connect to ap");
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  return wifiStatus;
}
