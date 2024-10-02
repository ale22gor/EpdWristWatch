#include "WifiEspWatch.h"

extern const char *TAG;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
  static int retries;
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
      esp_wifi_connect();
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");

      esp_wifi_connect();
      break;
    case WIFI_EVENT_AP_STACONNECTED:
      ESP_LOGI(TAG, "SoftAP transport: Connected!");
      break;
    case WIFI_EVENT_AP_STADISCONNECTED:
      ESP_LOGI(TAG, "SoftAP transport: Disconnected!");
      break;
    default:
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    /* Signal main application to continue execution */
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
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
      retries++;
      if (retries >= EXAMPLE_ESP_MAXIMUM_RETRY)
      {
        ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
        wifi_prov_mgr_reset_sm_state_on_failure();
        retries = 0;
      }
      break;
    }
    case WIFI_PROV_CRED_SUCCESS:
      ESP_LOGI(TAG, "Provisioning successful");
      retries = 0;
      break;
    case WIFI_PROV_END:
      /* De-initialize manager once provisioning is finished */
      wifi_prov_mgr_deinit();
      break;
    default:
      break;
    }
  }
}

esp_err_t appiid_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                    uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
  if (inbuf)
  {
    char appiid[inlen + 1] = "";
    memcpy(appiid, inbuf, inlen);
    appiid[inlen] = '\0';
    ESP_LOGI(TAG, "Received lattitude & longtitude data: %s", appiid);

    ESP_LOGI(TAG,"Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
      ESP_LOGI(TAG,"Done\n");

        // Write
      err = nvs_set_str(my_handle, "appiid", appiid);
      //ESP_LOGE((err != ESP_OK) ? TAG, "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      ESP_LOGI(TAG,"Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      //ESP_LOGE((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Close
      nvs_close(my_handle);
    }
  }
  char response[] = "SUCCESS";
  *outbuf = (uint8_t *)strdup(response);
  if (*outbuf == NULL)
  {
    ESP_LOGE(TAG, "System out of memory");
    return ESP_ERR_NO_MEM;
  }
  *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

  return ESP_OK;
}

esp_err_t latlong_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                    uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
  if (inbuf)
  {
    char latlong[inlen + 1] = "";
    memcpy(latlong, inbuf, inlen);
    latlong[inlen] = '\0';
    ESP_LOGI(TAG, "Received lattitude & longtitude data: %s", latlong);

    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
      ESP_LOGI(TAG,"Done\n");

        // Write
      err = nvs_set_str(my_handle, "latlong", latlong);
      //ESP_LOGE((err != ESP_OK) ? TAG, "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      ESP_LOGI(TAG,"Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      //ESP_LOGE((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Close
      nvs_close(my_handle);
    }
  }
  char response[] = "SUCCESS";
  *outbuf = (uint8_t *)strdup(response);
  if (*outbuf == NULL)
  {
    ESP_LOGE(TAG, "System out of memory");
    return ESP_ERR_NO_MEM;
  }
  *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

  return ESP_OK;
}

esp_err_t timezone_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                     uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
  if (inbuf)
  {
    char timezone[inlen + 1] = "";
    memcpy(timezone, inbuf, inlen);
    timezone[inlen] = '\0';
    ESP_LOGI(TAG, "Received timezone data: %s", timezone);

    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &my_handle);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
      ESP_LOGI(TAG,"Done\n");

      // Write
      err = nvs_set_str(my_handle, "timezone", timezone);
      //ESP_LOGE((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Commit written value.
      // After setting any values, nvs_commit() must be called to ensure changes are written
      // to flash storage. Implementations may write to storage at other times,
      // but this is not guaranteed.
      ESP_LOGI(TAG,"Committing updates in NVS ... ");
      err = nvs_commit(my_handle);
      //ESP_LOGE((err != ESP_OK) ? "Failed!\n" : "Done\n");

      // Close
      nvs_close(my_handle);
    }
  }
  char response[] = "SUCCESS";
  *outbuf = (uint8_t *)strdup(response);
  if (*outbuf == NULL)
  {
    ESP_LOGE(TAG, "System out of memory");
    return ESP_ERR_NO_MEM;
  }
  *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

  return ESP_OK;
}

void get_device_service_name(char *service_name, size_t max)
{
  uint8_t eth_mac[6];
  const char *ssid_prefix = "PROV_";
  esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  snprintf(service_name, max, "%s%02X%02X%02X",
           ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void wifi_init_sta(void)
{
  /* Start Wi-Fi in station mode */
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_disconnect(void)
{
  // vEventGroupDelete(s_wifi_event_group);
  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());
  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
  ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
}

void wifi_setDefaults()
{
  s_wifi_event_group = xEventGroupCreate();

  esp_netif_create_default_wifi_sta();
  esp_netif_t *p_netif = esp_netif_create_default_wifi_ap();

  esp_netif_ip_info_t ip_info = {};
  esp_netif_set_ip4_addr(&ip_info.ip, 10, 1, 2, 1); // ludite friendly IP address
  esp_netif_set_ip4_addr(&ip_info.gw, 10, 1, 2, 1);
  esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);

  esp_netif_dns_info_t dns;
  IP_ADDR4(&dns.ip, 8, 8, 8, 8);

  dhcps_offer_t dhcps_dns_value = OFFER_DNS;

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(p_netif));
  ESP_ERROR_CHECK(esp_netif_set_ip_info(p_netif, &ip_info));
  ESP_ERROR_CHECK(esp_netif_dhcps_option(p_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value)));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(p_netif));
}

bool wifi_update_prov_and_connect(bool reset)
{

  bool wifiStatus = false;

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_prov_mgr_config_t config = {
      .scheme = wifi_prov_scheme_softap,
      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};
  ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

  if (reset)
    ESP_ERROR_CHECK(wifi_prov_mgr_reset_provisioning());

  bool provisioned = false;
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
  ESP_LOGI(TAG, "WIFI init finished.");
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

    /* What is the security level that we want (0, 1, 2):
     *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
     *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
     *          using X25519 key exchange and proof of possession (pop) and AES-CTR
     *          for encryption/decryption of messages.
     *      - WIFI_PROV_SECURITY_2 SRP6a based authentication and key exchange
     *        + AES-GCM encryption/decryption of messages
     */
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

    /* Do we want a proof-of-possession (ignored if Security 0 is selected):
     *      - this should be a string with length > 0
     *      - NULL if not used
     */
    const char *pop = "abcd12345";

    /* This is the structure for passing security parameters
     * for the protocomm security 1.
     */
    wifi_prov_security1_params_t *sec_params = pop;

    const char *username = NULL;

    /* What is the service key (could be NULL)
     * This translates to :
     *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
     *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
     *     - simply ignored when scheme is wifi_prov_scheme_ble
     */
    const char *service_key = "abcd12345";

    /* An optional endpoint that applications can create if they expect to
     * get some additional custom data during provisioning workflow.
     * The endpoint name can be anything of your choice.
     * This call must be made before starting the provisioning.
     */
    // wifi_prov_mgr_endpoint_create("latitude");
    wifi_prov_mgr_endpoint_create("latlong-data");
    wifi_prov_mgr_endpoint_create("timezone-data");
    wifi_prov_mgr_endpoint_create("appiid-data");

    /* Do not stop and de-init provisioning even after success,
     * so that we can restart it later. */

    // wifi_prov_mgr_disable_auto_stop(1000);

    /* Start provisioning service */
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

    /* The handler for the optional endpoint created above.
     * This call must be made after starting the provisioning, and only if the endpoint
     * has already been created above.
     */
    wifi_prov_mgr_endpoint_register("latlong-data", latlong_prov_data_handler, NULL);
    wifi_prov_mgr_endpoint_register("timezone-data", timezone_prov_data_handler, NULL);
    wifi_prov_mgr_endpoint_register("appiid-data", appiid_prov_data_handler, NULL);

    // wifi_prov_mgr_endpoint_register("longitude", longitude_prov_data_handler, NULL);

    /* Uncomment the following to wait for the provisioning to finish and then release
     * the resources of the manager. Since in this case de-initialization is triggered
     * by the default event loop handler, we don't need to call the following */
    // wifi_prov_mgr_wait();
    // wifi_prov_mgr_deinit();
    /* Print QR code for provisioning */

    // wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_SOFTAP);
  }
  else
  {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
    /* We don't need the manager as device is already provisioned,
     * so let's release it's resources */
    wifi_prov_mgr_deinit();

    /* Start Wi-Fi station */
    wifi_init_sta();
  }

  /* Wait for Wi-Fi connection */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT,
                                         pdTRUE,
                                         pdTRUE,
                                         portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "Wifi connected");
    wifiStatus = true;
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  return wifiStatus;
}

void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport)
{
  if (!name || !transport)
  {
    ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
    return;
  }
  char payload[150] = {0};
  if (pop)
  {
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                       ",\"pop\":\"%s\",\"transport\":\"%s\"}",
             PROV_QR_VERSION, name, pop, transport);
  }
  else
  {
    snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\""
                                       ",\"transport\":\"%s\"}",
             PROV_QR_VERSION, name, transport);
  }
  ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  esp_qrcode_generate(&cfg, payload);
  ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload);
}