#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "WifiEspWatch.h"
#include "HttpEspWatch.h"
#include "SntpEspWatch.h"
#include "BatteryEspWatch.h"
#include "PrintEspWatch.h"

#include <ctime>
#include "driver/gpio.h"

#define PIN_MOTOR 4
#define PIN_KEY 35
#define PWR_EN 5
#define Backlight 33
#define Bat_ADC 34

struct tm timeinfo;

const char *TAG = "epd watch";

void UpdateTimeCallback(void *parameter);
void SyncDataCallback(void *parameter);
void TaskUpdateTime(void *pvParameters);
void Task_SyncData(void *pvParameters);
void Task_Sleep(void *pvParameters);

static EventGroupHandle_t minute_event_group;
static EventGroupHandle_t syncData_event_group;
static EventGroupHandle_t mainApp_event_group;

const int time_Update = BIT0;

#define MINUTE_UPDATE_BIT BIT1
#define SYNC_DATA_BIT BIT1

#define TIME_UPDATED_BIT BIT0
#define DATA_UPDATED_BIT BIT1

extern "C" void app_main()
{

  Serial.begin(115200);
  vTaskDelay(100);

  gpio_reset_pin(GPIO_NUM_5);
  // Настраиваем вывод PWR_EN на выход без подтяжки
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(GPIO_NUM_5, GPIO_FLOATING);
  gpio_set_level(GPIO_NUM_5, HIGH);

  gpio_reset_pin(GPIO_NUM_35);
  gpio_set_direction(GPIO_NUM_35, GPIO_MODE_INPUT);
  gpio_set_pull_mode(GPIO_NUM_35, GPIO_PULLUP_ONLY);

  vTaskDelay(250);
  Serial.setDebugOutput(true);

  // Set timezone
  setenv("TZ", "MSK-3", 1);
  tzset();

  vTaskDelay(250);

  initDisplay();
  vTaskDelay(1000);
  

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    /* NVS partition was truncated
     * and needs to be erased */
    ESP_ERROR_CHECK(nvs_flash_erase());

    /* Retry nvs_flash_init */
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  /* Initialize TCP/IP */
  ESP_ERROR_CHECK(esp_netif_init());

  /* Initialize the event loop */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  if (wifi_init_sta())
  {
    InitNtpTime();
    GET_Request();
  }
  wifi_disconnect();

  ESP_LOGI(TAG, "setup(): ready");

  vTaskDelay(100);

  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  initDisplayText();

  esp_timer_handle_t minuteClockTimer;
  // esp_timer_handle_t SyncDataTimer;

  esp_timer_create_args_t configMinuteClockTimer = {
      .callback = UpdateTimeCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "minute_Timer",
      .skip_unhandled_events = true,
  };
  /*
   esp_timer_create_args_t configSyncDataTimer = {
       .callback = SyncDataCallback,
       .arg = NULL,
       .dispatch_method = ESP_TIMER_TASK,
       .name = "syncData_Timer",
       .skip_unhandled_events = true,
   };
 */
  minute_event_group = xEventGroupCreate();
  // syncData_event_group = xEventGroupCreate();
  // mainApp_event_group = xEventGroupCreate();

  // xEventGroupSetBits(mainApp_event_group, DATA_UPDATED_BIT);
  esp_timer_create(&configMinuteClockTimer, &minuteClockTimer);
  // esp_timer_create(&configSyncDataTimer, &SyncDataTimer);

  xTaskCreate(&TaskUpdateTime, "Task_UpdateTime", 8192, NULL, 2, NULL);
  //  xTaskCreate(&Task_Sleep, "Task_Sleep", 1024, NULL, 2, NULL);
  //  xTaskCreate(&Task_SyncData, "Task_SyncData", 8192, NULL, 3, NULL);
  esp_timer_start_periodic(minuteClockTimer, 60 * 1000 * 1000);
  // esp_timer_start_periodic(SyncDataTimer, 60 * 1000 * 1000); // 10800000000L);

  esp_sleep_enable_timer_wakeup(55000000);

  esp_light_sleep_start();
}

void UpdateTimeCallback(void *parameter)
{
  xEventGroupSetBits(minute_event_group, MINUTE_UPDATE_BIT);
}
void SyncDataCallback(void *parameter)
{
  // xEventGroupClearBits(mainApp_event_group, DATA_UPDATED_BIT);
  xEventGroupSetBits(syncData_event_group, SYNC_DATA_BIT);
}

void TaskUpdateTime(void *parameter)
{

  while (true)
  {
    xEventGroupWaitBits(minute_event_group, MINUTE_UPDATE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "TaskUpdateTime");

    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_hour == 4 && timeinfo.tm_min == 30)
    {
      if (wifi_reconnect())
      {
        UpdateNtpTime();
        GET_Request();
      }
      wifi_disconnect();
    }
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0)
    {
      printDate(0, 70);
    }
    printHour(7, 0);

    if (timeinfo.tm_hour % 3 == 0 && timeinfo.tm_min == 0)
      printWeather(20, 5);

    if (timeinfo.tm_min % 5 == 0)
    {

      powerInit();
      int voltage = powerMeasure();
      printPower(voltage, 130, 150);
      stopPowerMeasure();
    }

    hibernateDisplay();
    vTaskDelay(250);

    esp_sleep_enable_timer_wakeup(55000000);

    esp_light_sleep_start();
  }
}
void Task_SyncData(void *parameter)
{
  while (true)
  {

    xEventGroupWaitBits(syncData_event_group, SYNC_DATA_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Task_SyncData");

    // wifi_reconect();
    vTaskDelay(250);

    // xEventGroupSetBits(mainApp_event_group, DATA_UPDATED_BIT);
  }
}
void Task_Sleep(void *parameter)
{
  while (true)
  {
    xEventGroupWaitBits(mainApp_event_group, DATA_UPDATED_BIT | TIME_UPDATED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    xEventGroupClearBits(mainApp_event_group, TIME_UPDATED_BIT);

    esp_sleep_enable_timer_wakeup(55000000);

    esp_light_sleep_start();
  }
}
