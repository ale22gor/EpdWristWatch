#include "WifiEspWatch.h"

static int s_retry_num = 0;

extern const char *TAG;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
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
  else if (event_base == WIFI_PROV_EVENT)
  {
    switch (event_id)
    {
    case WIFI_PROV_START:
      ESP_LOGI(TAG, "Provisioning started");
      break;
    case WIFI_PROV_CRED_RECV:
    {
      wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
      ESP_LOGI(TAG, "Received Wi-Fi credentials"
                    "\n\tSSID     : %s\n\tPassword : %s",
               (const char *)wifi_sta_cfg->ssid,
               (const char *)wifi_sta_cfg->password);
      break;
    }
    case WIFI_PROV_CRED_FAIL:
    {
      wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
      ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                    "\n\tPlease reset to factory and retry provisioning",
               (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
      break;
    }
    case WIFI_PROV_CRED_SUCCESS:
      ESP_LOGI(TAG, "Provisioning successful");
      break;
    case WIFI_PROV_END:
      /* De-initialize manager once provisioning is finished */
      wifi_prov_mgr_deinit();
      break;
    default:
      break;
    }
  }
  else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT)
  {
    switch (event_id)
    {
    case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
      ESP_LOGI(TAG, "BLE transport: Connected!");
      break;
    case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
      ESP_LOGI(TAG, "BLE transport: Disconnected!");
      break;
    default:
      break;
    }
  }
  else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT)
  {
    switch (event_id)
    {
    case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
      ESP_LOGI(TAG, "Secured session established!");
      break;
    case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
      ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
      break;
    case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
      ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!");
      break;
    default:
      break;
    }
  }
}

bool wifi_init_sta(void)
{
  bool wifiStatus = false;
  s_wifi_event_group = xEventGroupCreate();

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_event_handler,
                                             NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &wifi_event_handler,
                                             NULL));
  wifi_prov_mgr_config_t prov_config = {
      .scheme = wifi_prov_scheme_ble,
      .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM};

  /* Initialize provisioning manager with the
   * configuration parameters set above */
  ESP_ERROR_CHECK(wifi_prov_mgr_init(prov_config));

  bool provisioned = false;
  /* Let's find out if the device is provisioned */
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

  if (!provisioned)
  {
    ESP_LOGI(TAG, "Starting provisioning");

    /* What is the Device Service Name that we want
     * This translates to :
     *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
     *     - device name when scheme is wifi_prov_scheme_ble
     */
    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));
    /* What is the service key (could be NULL)
     * This translates to :
     *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
     *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
     *     - simply ignored when scheme is wifi_prov_scheme_ble
     */
    const char *service_key = NULL;

    /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
     * set a custom 128 bit UUID which will be included in the BLE advertisement
     * and will correspond to the primary GATT service that provides provisioning
     * endpoints as GATT characteristics. Each GATT characteristic will be
     * formed using the primary service UUID as base, with different auto assigned
     * 12th and 13th bytes (assume counting starts from 0th byte). The client side
     * applications must identify the endpoints by reading the User Characteristic
     * Description descriptor (0x2901) for each characteristic, which contains the
     * endpoint name of the characteristic */
    uint8_t custom_service_uuid[] = {
        /* LSB <---------------------------------------
         * ---------------------------------------> MSB */
        0xb4,
        0xdf,
        0x5a,
        0x1c,
        0x3f,
        0x6b,
        0xf4,
        0xbf,
        0xea,
        0x4a,
        0x82,
        0x03,
        0x04,
        0x90,
        0x1a,
        0x02,
    };

    /* If your build fails with linker errors at this point, then you may have
     * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
     * the sdkconfig.defaults in the example project) */
    wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

    /* An optional endpoint that applications can create if they expect to
     * get some additional custom data during provisioning workflow.
     * The endpoint name can be anything of your choice.
     * This call must be made before starting the provisioning.
     */
    wifi_prov_mgr_endpoint_create("custom-data");

    /* Start provisioning service */
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, service_key));

    /* The handler for the optional endpoint created above.
     * This call must be made after starting the provisioning, and only if the endpoint
     * has already been created above.
     */
    wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

    /* Uncomment the following to wait for the provisioning to finish and then release
     * the resources of the manager. Since in this case de-initialization is triggered
     * by the default event loop handler, we don't need to call the following */
    // wifi_prov_mgr_wait();
    // wifi_prov_mgr_deinit();
    /* Print QR code for provisioning */
    wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);
  }
  else
  {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

    /* We don't need the manager as device is already provisioned,
     * so let's release it's resources */
    wifi_prov_mgr_deinit();

    /* Start Wi-Fi station */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
  }

  ESP_LOGI(TAG, "wifi_init_sta finished.");

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

void wifi_disconnect(void)
{
  // vEventGroupDelete(s_wifi_event_group);
  ESP_ERROR_CHECK(esp_wifi_stop());
}

bool wifi_reconnect(void)
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

  ESP_LOGI(TAG, "Reconect init finished.");

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