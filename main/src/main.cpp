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

static EventGroupHandle_t minute_event_group;

static QueueHandle_t button_queue = NULL;
esp_timer_handle_t buttonTimer;

const int time_Update = BIT0;

#define MINUTE_UPDATE_BIT BIT1
#define SYNC_DATA_BIT BIT1

#define TIME_UPDATED_BIT BIT0
#define DATA_UPDATED_BIT BIT1

#define MAIN_SCR_PRINT BIT0
#define MINUTE_PRINT_UPD_MAIN_SCR BIT1
#define MENU_PRINT BIT2
#define MENU_PRINT_NEXT BIT3

/* Stores the handle of the task that will be notified when the
   transmission is complete. */
static TaskHandle_t xTskUpdMainScrToNotify = NULL, xTskMenuToNotify = NULL;

/* The index within the target task's array of task notifications
   to use. */
const UBaseType_t xArrayMinuteUpdIndex = 0;
const UBaseType_t xArrayMenuScrIndex = 2;

bool dataUpdated = false;
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
    bool button_pressed;
    // Ждем события нажатия кнопки в очереди
    ulTaskNotifyTake(pdTRUE,
                     portMAX_DELAY);

    ESP_LOGI("ISR", "Button is pressed");

    if (currScreenState == MainScreen)
    {
      if (xTaskNotify(xTskUpdMainScrToNotify,
                      MENU_PRINT,
                      eSetValueWithoutOverwrite) == pdPASS)
      {
        currScreenState = MenuScreen;
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
            dataUpdated = true;
          }
          wifi_disconnect();
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
        if (xTaskNotify(xTskUpdMainScrToNotify,
                        MENU_PRINT_NEXT,
                        eSetValueWithoutOverwrite) == pdPASS)
        {
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
        }
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
  setenv("TZ", "MSK-2", 1);
  tzset();

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

  // Создаем входящую очередь задачи
  button_queue = xQueueCreate(32, sizeof(bool));

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

  esp_pm_config_t pm_config = {
      .max_freq_mhz = 160,
      .min_freq_mhz = 80,
      .light_sleep_enable = true};
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  ESP_LOGI(TAG, "setup(): ready");

  vTaskDelay(100);

  time_t now = time(nullptr);
  localtime_r(&now, &timeinfo);
  initDisplayText();
  hibernateDisplay();

  esp_timer_handle_t minuteClockTimer;

  esp_timer_create_args_t configMinuteClockTimer = {
      .callback = UpdateTimeCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "minute_Timer",
      .skip_unhandled_events = true,
  };

  minute_event_group = xEventGroupCreate();

  esp_timer_create(&configMinuteClockTimer, &minuteClockTimer);

  xTaskCreate(&TaskUpdateTime, "Task_UpdateTime", 8192, NULL, 2, &xTskUpdMainScrToNotify);
  esp_timer_start_periodic(minuteClockTimer, 60 * 1000 * 1000);
}

void UpdateTimeCallback(void *parameter)
{
  // xEventGroupSetBits(minute_event_group, MINUTE_UPDATE_BIT);
  /* Notify the task that the transmission is complete. */
  xTaskNotify(xTskUpdMainScrToNotify,
              MINUTE_PRINT_UPD_MAIN_SCR,
              eSetValueWithoutOverwrite);
}

void TaskUpdateTime(void *parameter)
{

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
      time_t now = time(nullptr);
      localtime_r(&now, &timeinfo);

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
        updateMenu(1);
      }
      break;
      case Update:
      {
        updateMenu(2);
      }
      break;
      case Weather:
      {
        updateMenu(3);
      }
      break;
      case Back:
      {
        updateMenu(0);
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
