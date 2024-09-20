#include "SntpEspWatch.h"

extern const char *TAG;

void UpdateNtpTime()
{
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);

  int retry = 0;
  const int retry_count = 15;

  while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) != ESP_OK && ++retry < retry_count)
  {
    ESP_LOGI(TAG, "Getting Time");
    vTaskDelay(1000);
  }
  esp_netif_sntp_deinit();
}