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

#include "Definitions.h"

const char *weatherDtKey[WEATHERTIMESTUMPS]{
    "weatherDt_1",
    "weatherDt_2",
    "weatherDt_3",
    "weatherDt_4",
    "weatherDt_5",
    "weatherDt_6",
    "weatherDt_7",
    "weatherDt_8",
    "weatherDt_9",
    "weatherDt_10",
    "weatherDt_11",
    "weatherDt_12",
    "weatherDt_13",
    "weatherDt_14",
    "weatherDt_15",
    "weatherDt_16",
    "weatherDt_17",
    "weatherDt_18",
    "weatherDt_19",
    "weatherDt_20",
    "weatherDt_21",
    "weatherDt_22",
    "weatherDt_23",
    "weatherDt_24",
    "weatherDt_25",
    "weatherDt_26",
    "weatherDt_27",
    "weatherDt_28",
    "weatherDt_29",
    "weatherDt_30",
    "weatherDt_31",
    "weatherDt_32",
    "weatherDt_33",
    "weatherDt_34",
    "weatherDt_35",
    "weatherDt_36",
    "weatherDt_37",
    "weatherDt_38",
    "weatherDt_39",
    "weatherDt_40",
};
const char *weatherTempKey[WEATHERTIMESTUMPS]{
    "weatherTemp_1",
    "weatherTemp_2",
    "weatherTemp_3",
    "weatherTemp_4",
    "weatherTemp_5",
    "weatherTemp_6",
    "weatherTemp_7",
    "weatherTemp_8",
    "weatherTemp_9",
    "weatherTemp_10",
    "weatherTemp_11",
    "weatherTemp_12",
    "weatherTemp_13",
    "weatherTemp_14",
    "weatherTemp_15",
    "weatherTemp_16",
    "weatherTemp_17",
    "weatherTemp_18",
    "weatherTemp_19",
    "weatherTemp_20",
    "weatherTemp_21",
    "weatherTemp_22",
    "weatherTemp_23",
    "weatherTemp_24",
    "weatherTemp_25",
    "weatherTemp_26",
    "weatherTemp_27",
    "weatherTemp_28",
    "weatherTemp_29",
    "weatherTemp_30",
    "weatherTemp_31",
    "weatherTemp_32",
    "weatherTemp_33",
    "weatherTemp_34",
    "weatherTemp_35",
    "weatherTemp_36",
    "weatherTemp_37",
    "weatherTemp_38",
    "weatherTemp_39",
    "weatherTemp_40",
};
const char *weatherMainKey[WEATHERTIMESTUMPS]{
    "weatherMain_1",
    "weatherMain_2",
    "weatherMain_3",
    "weatherMain_4",
    "weatherMain_5",
    "weatherMain_6",
    "weatherMain_7",
    "weatherMain_8",
    "weatherMain_9",
    "weatherMain_10",
    "weatherMain_11",
    "weatherMain_12",
    "weatherMain_13",
    "weatherMain_14",
    "weatherMain_15",
    "weatherMain_16",
    "weatherMain_17",
    "weatherMain_18",
    "weatherMain_19",
    "weatherMain_20",
    "weatherMain_21",
    "weatherMain_22",
    "weatherMain_23",
    "weatherMain_24",
    "weatherMain_25",
    "weatherMain_26",
    "weatherMain_27",
    "weatherMain_28",
    "weatherMain_29",
    "weatherMain_30",
    "weatherMain_31",
    "weatherMain_32",
    "weatherMain_33",
    "weatherMain_34",
    "weatherMain_35",
    "weatherMain_36",
    "weatherMain_37",
    "weatherMain_38",
    "weatherMain_39",
    "weatherMain_40",
};

#define PIN_MOTOR 4
#define PIN_KEY 35
#define PWR_EN 5
#define Backlight 33
#define Bat_ADC 34

struct tm timeinfo = {0};
struct tm sunriseTm = {0};
struct tm sunsetTm = {0};

weatherData weatherArray[WEATHERTIMESTUMPS];
int weatherScreenPage = 0;
int currentweatherIndex = 0;

const char *TAG = "EPDWatchMain";
const int weatherTimestumps = 40;

void UpdateTimeCallback(void *parameter);
void TaskUpdateData(void *parameter);
void TaskPrintScreen(void *pvParameters);
void GetWeatherFromNVS();
void UpdateLocationFromNVS();
void UpdateTimezoneFromNVS();

void MenuSwitchNext();
void FullScreenPrint();

esp_timer_handle_t buttonTimer;

#define MAIN_SCR_PRINT BIT0
#define MINUTE_PRINT_UPD_MAIN_SCR BIT1
#define MENU_PRINT BIT2
#define MENU_PRINT_NEXT BIT3
#define PRINT_PROV_AND_UPD_SCR BIT4
#define PRINT_UPD_SCR BIT5
#define PRINT_WEATHER_SCR BIT6

#define PROV_AND_UPDATE BIT1
#define JUST_UPDATE BIT2

/* Stores the handle of the task that will be notified when the
   transmission is complete. */
static TaskHandle_t xTskPrintScrNotify = NULL, xTskButtonHandlerNotify = NULL, xTskUpdateDataNotify = NULL;

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
  MenuScreen,
  WeatherScreen
};
screenSTate currScreenState = MainScreen;

esp_timer_handle_t minuteClockTimer;

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
  vTaskNotifyGiveFromISR(xTskButtonHandlerNotify,
                         &xHigherPriorityTaskWoken);

  // Если высокоприоритетная задача ждет этого события, переключаем управление
  // if (xResult == pdPASS)
  // {
  //  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  //};
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Функция задачи светодиода
void TaskButtonHandler(void *pvParameters)
{

  while (1)
  {
    // Ждем события нажатия кнопки в очереди
    ulTaskNotifyTake(pdTRUE,
                     portMAX_DELAY);

    esp_timer_stop(minuteClockTimer);

    ESP_LOGI(TAG, "Button is pressed");

    if (currScreenState == MainScreen)
    {
      if (xTaskNotify(xTskPrintScrNotify,
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
          xTaskNotify(xTskPrintScrNotify,
                      PRINT_PROV_AND_UPD_SCR,
                      eSetValueWithoutOverwrite);

          // xTaskNotify(xTskUpdateDataNotify,
          //             PROV_AND_UPDATE,
          //             eSetValueWithoutOverwrite);

          // ulTaskNotifyTakeIndexed(1, pdTRUE,
          //                         portMAX_DELAY);

          if (wifi_update_prov_and_connect(true))
          {
            UpdateNtpTime();
            GET_Request();
            GetWeatherFromNVS();
            UpdateLocationFromNVS();
            UpdateTimezoneFromNVS();
          }

          wifi_disconnect();

          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);

          if (xTaskNotify(xTskPrintScrNotify,
                          MAIN_SCR_PRINT,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = MainScreen;
          }
        }
        break;
        case Update:
        {

          xTaskNotify(xTskPrintScrNotify,
                      PRINT_UPD_SCR,
                      eSetValueWithoutOverwrite);

          // xTaskNotify(xTskUpdateDataNotify,
          //             JUST_UPDATE,
          //             eSetValueWithoutOverwrite);

          // ulTaskNotifyTakeIndexed(1, pdTRUE,
          //                         portMAX_DELAY);

          if (wifi_update_prov_and_connect(false))
          {
            UpdateNtpTime();
            GET_Request();
            GetWeatherFromNVS();
            UpdateLocationFromNVS();
          }

          wifi_disconnect();

          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);

          if (xTaskNotify(xTskPrintScrNotify,
                          MAIN_SCR_PRINT,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = MainScreen;
          }
        }
        break;
        case Weather:
        {
          if (xTaskNotify(xTskPrintScrNotify,
                          PRINT_WEATHER_SCR,
                          eSetValueWithoutOverwrite) == pdPASS)
          {
            currScreenState = WeatherScreen;
          }
        }
        break;
        case Back:
        {
          time_t now = time(nullptr);
          localtime_r(&now, &timeinfo);
          if (xTaskNotify(xTskPrintScrNotify,
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
        MenuSwitchNext();
        xTaskNotify(xTskPrintScrNotify,
                    MENU_PRINT_NEXT,
                    eSetValueWithoutOverwrite);
      }
    }
    else if (currScreenState == WeatherScreen)
    {

      vTaskDelay(200);
      if (gpio_get_level(GPIO_NUM_35) == 0)
      {
        time_t now = time(nullptr);
        localtime_r(&now, &timeinfo);
        if (xTaskNotify(xTskPrintScrNotify,
                        MAIN_SCR_PRINT,
                        eSetValueWithoutOverwrite) == pdPASS)
        {
          currScreenState = MainScreen;
          weatherScreenPage = 0;
        }
      }
      else
      {
        if (weatherScreenPage >= 3)
        {
          weatherScreenPage = 0;
        }
        else
        {
          weatherScreenPage++;
        }
        xTaskNotify(xTskPrintScrNotify,
                    PRINT_WEATHER_SCR,
                    eSetValueWithoutOverwrite);
      }
    }

    esp_timer_start_periodic(minuteClockTimer, 60 * 1000 * 1000);
  };
  vTaskDelete(NULL);
}

void GetWeatherFromNVS()
{
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("weather", NVS_READONLY, &my_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAG, "Done\n");

    ESP_LOGI(TAG, "Reading weather data from NVS ... ");

    // Example of listing all the key-value pairs of any type under specified handle (which defines a partition and namespace)

    for (int i = 0; i < WEATHERTIMESTUMPS; i++)
    {
      int64_t dt;
      err = nvs_get_i64(my_handle, weatherDtKey[i], &dt);
      localtime_r(&dt, &weatherArray[i].time);
      nvs_get_i8(my_handle, weatherTempKey[i], &weatherArray[i].temp);
      nvs_get_u16(my_handle, weatherMainKey[i], &weatherArray[i].weather);
    }
  }

  nvs_close(my_handle);
}

int FindWeatherIndex()
{

  for (int i = 0; i < WEATHERTIMESTUMPS; i++)
  {
    time_t dt = mktime(&weatherArray[i].time);
    int64_t currTime = time(nullptr);
    if (dt - 10800 < currTime && dt > currTime)
    {
      return i;
    }
  }

  return -1;
}

void UpdateLocationFromNVS()
{
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("location", NVS_READONLY, &my_handle);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAG, "Done\n");

    ESP_LOGI(TAG, "Reading sunrise & sunset from NVS ... ");

    int64_t sunrise;
    err = nvs_get_i64(my_handle, "sunrise", &sunrise);

    switch (err)
    {
    case ESP_OK:
    {
      ESP_LOGI(TAG, "Done\n");
      time_t sunriseTime_t = (time_t)sunrise;
      sunriseTm = *localtime(&sunriseTime_t);
      break;
    }
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    int64_t sunset;
    err = nvs_get_i64(my_handle, "sunset", &sunset);

    switch (err)
    {
    case ESP_OK:
    {
      ESP_LOGI(TAG, "Done\n");
      time_t sunsetTime_t = (time_t)sunset;
      sunsetTm = *localtime(&sunsetTime_t);
      break;
    }
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }
  }
  nvs_close(my_handle);
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

  // ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_setDefaults();

  xTaskCreate(&TaskButtonHandler, "Task_ButtonHandler", 8192, NULL, 3, &xTskButtonHandlerNotify);

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

  GetWeatherFromNVS();

  FullScreenPrint();
  hibernateDisplay();

  esp_timer_create_args_t configMinuteClockTimer = {
      .callback = UpdateTimeCallback,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "minute_Timer",
      .skip_unhandled_events = true,
  };

  esp_timer_create(&configMinuteClockTimer, &minuteClockTimer);

  xTaskCreate(&TaskPrintScreen, "Task_PrintScreen", 6144, NULL, 2, &xTskPrintScrNotify);
  // xTaskCreate(&TaskUpdateData, "Task_UpdateData", 10240, NULL, 3, &xTskUpdateDataNotify);

  esp_timer_start_periodic(minuteClockTimer, 60 * 1000 * 1000);

  esp_sleep_enable_gpio_wakeup();

  gpio_wakeup_enable(GPIO_NUM_35, GPIO_INTR_LOW_LEVEL);

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
  xTaskNotify(xTskPrintScrNotify,
              MINUTE_PRINT_UPD_MAIN_SCR,
              eSetValueWithoutOverwrite);
}

void TaskPrintScreen(void *parameter)
{
  uint32_t ulNotifiedValue;

  while (true)
  {
    ESP_LOGI(TAG, "TaskPrintScreen");
    // xEventGroupWaitBits(minute_event_group, MINUTE_UPDATE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    xTaskNotifyWait(0x00,             /* Don't clear any notification bits on entry. */
                    ULONG_MAX,        /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue, /* Notified value pass out in
                                         ulNotifiedValue. */
                    portMAX_DELAY);

    if (ulNotifiedValue == MINUTE_PRINT_UPD_MAIN_SCR && currScreenState == MainScreen)
    {

      if ((timeinfo.tm_hour % 3 == 0 && timeinfo.tm_min == 0) || (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0))
      {
        FullScreenPrint();
      }
      else if (timeinfo.tm_min % 5 == 0)
      {
        powerInit();
        int voltage = powerMeasure();
        printPower(voltage, 130, 150);
        stopPowerMeasure();
        printHour(7, 0, timeinfo);
      }
      else
      {
        printHour(7, 0, timeinfo);
      }
    }
    else if (ulNotifiedValue == MAIN_SCR_PRINT)
    {
      FullScreenPrint();
    }
    else if (ulNotifiedValue == MENU_PRINT)
    {
      displayMenu();
    }
    else if (ulNotifiedValue == MENU_PRINT_NEXT && currScreenState == MenuScreen)
    {
      updateMenu(int(currMenuState));
    }
    else if (ulNotifiedValue == PRINT_PROV_AND_UPD_SCR)
    {
      printProvAndUpdate();
    }
    else if (ulNotifiedValue == PRINT_UPD_SCR)
    {
      printUpdate();
    }
    else if (ulNotifiedValue == PRINT_WEATHER_SCR)
    {
      printWeather(weatherArray, weatherScreenPage);
    }
    hibernateDisplay();
    vTaskDelay(250);
  }
}

void MenuSwitchNext()
{
  switch (currMenuState)
  {
  case menuState::ProvAndUpdate:
  {
    currMenuState = menuState::Update;
  }
  break;
  case menuState::Update:
  {
    currMenuState = menuState::Weather;
  }
  break;
  case menuState::Weather:
  {
    currMenuState = menuState::Back;
  }
  break;
  case Back:
  {
    currMenuState = menuState::ProvAndUpdate;
  }
  break;
  default:
    break;
  }
}

void FullScreenPrint()
{
  int i = FindWeatherIndex();
  if (i >= 0)
  {
    initDisplayText(timeinfo, sunriseTm, sunsetTm, weatherArray[i]);
  }
  else
  {
    struct weatherData weatherTmp = {.weather = 0, .temp = 0, .time = 0};
    initDisplayText(timeinfo, sunriseTm, sunsetTm, weatherTmp);
  }
}

void TaskUpdateData(void *parameter)
{
  uint32_t ulNotifiedValue;

  // ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_setDefaults();

  while (1)
  {

    xTaskNotifyWait(0x00,             /* Don't clear any notification bits on entry. */
                    ULONG_MAX,        /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue, /* Notified value pass out in
                                         ulNotifiedValue. */
                    portMAX_DELAY);

    vTaskDelay(2000);

    ESP_LOGI(TAG, "Update data task");

    if (ulNotifiedValue == PROV_AND_UPDATE)
    {
      if (wifi_update_prov_and_connect(true))
      {
        UpdateNtpTime();
        GET_Request();
        GetWeatherFromNVS();
        UpdateLocationFromNVS();
      }
    }
    else if (ulNotifiedValue == JUST_UPDATE)
    {
      if (wifi_update_prov_and_connect(false))
      {
        UpdateNtpTime();
        GET_Request();
        GetWeatherFromNVS();
        UpdateLocationFromNVS();
      }
    }
    wifi_disconnect();

    time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);

    xTaskNotifyGiveIndexed(xTskButtonHandlerNotify,
                           1);
  }
}

void UpdateTimezoneFromNVS(){
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("location", NVS_READONLY, &my_handle);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    ESP_LOGI(TAG, "Done\n");

    ESP_LOGI(TAG, "Reading timezone from NVS ... ");

    int16_t timezone;
    err = nvs_get_i16(my_handle, "timezone", &timezone);

    switch (err)
    {
    case ESP_OK:
    {
    ESP_LOGI(TAG, "Done\n");
    int8_t timezoneInHour = timezone / 3600;
    if(timezoneInHour == 2)
      setenv("TZ", "MSK-2", 1);
    else if(timezoneInHour == 3)
      setenv("TZ", "MSK-3", 1);
    else 
      setenv("TZ", "MSK-3", 1);

    break;
    }
    case ESP_ERR_NVS_NOT_FOUND:
      ESP_LOGI(TAG, "The value is not initialized yet!\n");
      break;
    default:
      ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    }

    
  }
  nvs_close(my_handle);
}

