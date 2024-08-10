#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_pm.h"

#include <ctime>
#include "driver/gpio.h"

#include "WifiEspWatch.h"
#include "HttpEspWatch.h"
#include "SntpEspWatch.h"
#include "BatteryEspWatch.h"
#include "PrintEspWatch.h"

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

esp_timer_handle_t buttonTimer;

#define MAIN_SCR_PRINT BIT0
#define MINUTE_PRINT_UPD_MAIN_SCR BIT1
#define MENU_PRINT BIT2
#define MENU_PRINT_NEXT BIT3

/* Stores the handle of the task that will be notified when the
   transmission is complete. */
static TaskHandle_t xTskUpdMainScrToNotify = NULL, xTskMenuToNotify = NULL;

enum menuState
{
  ProvAndUpdate,
  Update,
  Weather,
  Back
};
menuState currMenuState = ProvAndUpdate;
enum screenSTate
{
  MainScreen,
  MenuScreen
};
screenSTate currScreenState = MainScreen;

void ButtonTimerCallback(void *parameter)
{
  // ESP_LOGI("ISR", "Button is enabled");

  // Включаем прерывания на кнопке
  gpio_intr_enable(GPIO_NUM_35);
}

// Обработчик прерывания по нажатию кнопки
static void IRAM_ATTR isrButtonPress(void *arg)
{
  gpio_intr_disable(GPIO_NUM_35);
  esp_timer_start_once(buttonTimer, 2000000);

  // Переменные для переключения контекста
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // Поскольку мы "подписались" только на GPIO_INTR_NEGEDGE, мы уверены что это именно момент нажатия на кнопку
  // bool pressed = true;
  // Отправляем в очередь задачи событие "кнопка нажата"
  // xResult = xQueueSendFromISR(button_queue, &pressed, &xHigherPriorityTaskWoken);
  vTaskNotifyGiveFromISR(xTskMenuToNotify,
                         &xHigherPriorityTaskWoken);

  // Если высокоприоритетная задача ждет этого события, переключаем управление
  // if (xResult == pdPASS)
  // {
  //  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  //};
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Функция задачи светодиода
void TaskWifiUpdateData(void *pvParameters)
{

  while (1)
  {
    // Ждем события нажатия кнопки в очереди
    ulTaskNotifyTake(pdTRUE,
                     portMAX_DELAY);

    ESP_LOGI(TAG, "Button is pressed");

    if (currScreenState == MainScreen)
    {
      if (xTaskNotify(xTskUpdMainScrToNotify,
                      MENU_PRINT,
                      eSetValueWithoutOverwrite) == pdPASS)
      {
        currScreenState = MenuScreen;
        currMenuState = ProvAndUpdate;
      }
    }
    else if (currScreenState == MenuScreen)
    {
      vTaskDelay(200);
      if (gpio_get_level(GPIO_NUM_35) == 0)
      {
        switch (currMenuState)
        {
        case ProvAndUpdate:
        {
          if (wifi_update_prov_and_connect())
          {
            InitNtpTime();
            GET_Request();
          }
          wifi_disconnect();
          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);
          if (xTaskNotify(xTskUpdMainScrToNotify,
                          MAIN_SCR_PRINT,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = MainScreen;
          }
        }
        break;
        case Update:
        {
          if (wifi_connect())
          {
            InitNtpTime();
            GET_Request();
          }
          wifi_disconnect();
          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);
          if (xTaskNotify(xTskUpdMainScrToNotify,
                          MAIN_SCR_PRINT,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = MainScreen;
          }
        }
        break;
        case Weather:
        {
        }
        break;
        case Back:
        {
          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);
          if (xTaskNotify(xTskUpdMainScrToNotify,
                          MAIN_SCR_PRINT,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = MainScreen;
          }
        }
        break;
        default:
          break;
        }
      }
      else
      {
        ESP_LOGI(TAG, "Menu next click");

        switch (currMenuState)
        {
        case ProvAndUpdate:
        {
          currMenuState = Update;
        }
        break;
        case Update:
        {
          currMenuState = Weather;
        }
        break;
        case Weather:
        {
          currMenuState = Back;
        }
        break;
        case Back:
        {
          currMenuState = ProvAndUpdate;
        }
        break;
        default:
          break;
        }
        xTaskNotify(xTskUpdMainScrToNotify,
                    MENU_PRINT_NEXT,
                    eSetValueWithoutOverwrite);
      }
    }
  };
  vTaskDelete(NULL);
}

extern "C" void app_main()
{

  Serial.begin(115200);
  vTaskDelay(100);

  gpio_reset_pin(GPIO_NUM_5);
  // Настраиваем вывод PWR_EN на выход без подтяжки
  gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
  gpio_set_pull_mode(GPIO_NUM_5, GPIO_FLOATING);
  gpio_set_level(GPIO_NUM_5, HIGH);

  vTaskDelay(250);
  Serial.setDebugOutput(true);

  // Set timezone
  setenv("TZ", "MSK-3", 1);
  tzset();
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);

  vTaskDelay(250);

  ESP_LOGI(TAG, "Initialize Display");

  initDisplay();
  vTaskDelay(1000);
  ESP_LOGI(TAG, "Initialize NVS");

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

  ESP_LOGI(TAG, "Initialize TCP/IP");

  /* Initialize TCP/IP */
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_LOGI(TAG, "Initialize the event loop");

  /* Initialize the event loop */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();

  xTaskCreate(&TaskWifiUpdateData, "Task_WifiUpdateData", 8192, NULL, 3, &xTskMenuToNotify);

  gpio_reset_pin(GPIO_NUM_35);
  // gpio_pad_select_gpio(GPIO_NUM_35);
  gpio_set_direction(GPIO_NUM_35, GPIO_MODE_INPUT);
  gpio_set_pull_mode(GPIO_NUM_35, GPIO_PULLUP_ONLY);

  esp_timer_create_args_t configButtonTimer = {
      .callback = ButtonTimerCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "button_Timer",
      .skip_unhandled_events = true,
  };

  esp_timer_create(&configButtonTimer, &buttonTimer);

  esp_err_t err = gpio_install_isr_service(0);
  if (err == ESP_ERR_INVALID_STATE)
  {
    ESP_LOGW("ISR", "GPIO isr service already installed");
  };

  // Регистрируем обработчик прерывания на нажатие кнопки
  gpio_isr_handler_add(GPIO_NUM_35, isrButtonPress, NULL);

  // Устанавливаем тип события для генерации прерывания - по низкому уровню
  gpio_set_intr_type(GPIO_NUM_35, GPIO_INTR_NEGEDGE);

  // Разрешаем использование прерываний
  gpio_intr_enable(GPIO_NUM_35);

  vTaskDelay(100);

  esp_timer_handle_t minuteClockTimer;

  esp_timer_create_args_t configMinuteClockTimer = {
      .callback = UpdateTimeCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "minute_Timer",
      .skip_unhandled_events = true,
  };

  esp_timer_create(&configMinuteClockTimer, &minuteClockTimer);

  xTaskCreate(&TaskUpdateTime, "Task_UpdateTime", 8192, NULL, 2, &xTskUpdMainScrToNotify);
  esp_timer_start_periodic(minuteClockTimer, 60 * 1000 * 1000);

  esp_pm_config_t pm_config = {
      .max_freq_mhz = 160,
      .min_freq_mhz = 80,
      .light_sleep_enable = true};
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
  ESP_LOGI(TAG, "setup(): ready");
}

void UpdateTimeCallback(void *parameter)
{
  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  // xEventGroupSetBits(minute_event_group, MINUTE_UPDATE_BIT);
  /* Notify the task that the transmission is complete. */
  xTaskNotify(xTskUpdMainScrToNotify,
              MINUTE_PRINT_UPD_MAIN_SCR,
              eSetValueWithoutOverwrite);
}

void TaskUpdateTime(void *parameter)
{

  initDisplayText();
  hibernateDisplay();

  esp_sleep_enable_gpio_wakeup();

  gpio_wakeup_enable(GPIO_NUM_35, GPIO_INTR_LOW_LEVEL);

  uint32_t ulNotifiedValue;

  while (true)
  {
    ESP_LOGI(TAG, "TaskUpdateTime");
    // xEventGroupWaitBits(minute_event_group, MINUTE_UPDATE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    xTaskNotifyWait(0x00,             /* Don't clear any notification bits on entry. */
                    ULONG_MAX,        /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue, /* Notified value pass out in
                                         ulNotifiedValue. */
                    portMAX_DELAY);

    if (ulNotifiedValue == MINUTE_PRINT_UPD_MAIN_SCR && currScreenState == MainScreen)
    {

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
    }
    else if (ulNotifiedValue == MAIN_SCR_PRINT)
    {
      initDisplayText();
    }
    else if (ulNotifiedValue == MENU_PRINT)
    {
      displayMenu();
    }
    else if (ulNotifiedValue == MENU_PRINT_NEXT && currScreenState == MenuScreen)
    {
      switch (currMenuState)
      {
      case ProvAndUpdate:
      {
        updateMenu(0);
      }
      break;
      case Update:
      {
        updateMenu(1);
      }
      break;
      case Weather:
      {
        updateMenu(2);
      }
      break;
      case Back:
      {
        updateMenu(3);
      }
      break;
      default:
        break;
      }
    }
    // esp_sleep_enable_timer_wakeup(55000000);

    // esp_light_sleep_start();
  }
}
