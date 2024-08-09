#include "PrintEspWatch.h"

extern struct tm timeinfo;
extern weather localWeather;

SPIClass hspi(HSPI);

GxEPD2_BW<GxEPD2_150_BN, GxEPD2_150_BN::HEIGHT> display(
    GxEPD2_150_BN(EPD_CS, EPD_DC, EPD_RESET, EPD_BUSY));

void printHour(uint16_t x, uint16_t y)
{
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(7);
    display.setPartialWindow(10, 5, 175, 50);
    display.firstPage();
    do
    {
        display.setCursor(10, 5);
        display.fillScreen(GxEPD_WHITE);
        display.printf("%02d", timeinfo.tm_hour);
        display.setCursor(80, 5);
        display.print(":");
        display.setCursor(110, 5);
        display.printf("%02d", timeinfo.tm_min);
    } while (display.nextPage());
}

void printMinute(uint16_t x, uint16_t y)
{
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(7);
    display.setPartialWindow(x, y, 80, 50);
    display.firstPage();
    do
    {
        display.setCursor(x, y);
        display.fillScreen(GxEPD_WHITE);
        display.printf("%02d", timeinfo.tm_min);
    } while (display.nextPage());
}

void printDate(uint16_t x, uint16_t y)
{
    display.setTextColor(GxEPD_BLACK);
    display.setPartialWindow(0, 70, 70, 75);
    display.firstPage();

    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.setTextSize(3);
        display.setCursor(17, 70);
        display.println(&timeinfo, "%a");
        display.println(&timeinfo, "%d");
        display.print(&timeinfo, "%Y");

        display.setCursor(35, 95);
        display.setTextSize(2);
        display.print(&timeinfo, "%b");
    } while (display.nextPage());
}

void printPower(int voltage, uint16_t x, uint16_t y)
{

    int percentBat = voltage > 2420 ? 100 : (voltage / 25);

    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(1);
    display.setPartialWindow(x - 25, y, 75, 20);
    display.firstPage();

    int xWidth;
    if (percentBat > 95)
        xWidth = 45;
    else if (percentBat > 75)
        xWidth = 35;
    else if (percentBat > 40)
        xWidth = 25;
    else if (percentBat > 25)
        xWidth = 10;
    else
        xWidth = 5;

    do
    {
        display.fillScreen(GxEPD_WHITE);

        display.drawRect(x, y, 50, 20, GxEPD_BLACK);
        // display.drawRect(x + 28, y + 3, 3, 6, GxEPD_BLACK);

        display.fillRect(x, y, xWidth, 20, GxEPD_BLACK);

        display.setCursor(x - 25, y);
        display.print(percentBat);
        display.print("%");

    } while (display.nextPage());
}

void printWeather(uint16_t x, uint16_t y)
{
    if (localWeather.weatherDataQueue.empty())
        return;

    display.setPartialWindow(0, 150, 100, 50);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(3);
    display.setCursor(0, 150);

    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.print(localWeather.weatherDataQueue.front().temp);

    } while (display.nextPage());

    localWeather.weatherDataQueue.pop();
}

void initDisplayText()
{

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    // display.setTextWrap(false);

    // print time

    display.setCursor(10, 5);
    display.setTextSize(7);
    display.printf("%02d", timeinfo.tm_hour);
    display.setCursor(80, 5);
    display.print(":");
    display.setCursor(110, 5);
    display.printf("%02d", timeinfo.tm_min);

    display.setCursor(10, 55);
    display.setTextSize(1);
    display.print("time");

    display.fillTriangle(0, 53, 6, 58, 0, 63, GxEPD_BLACK);

    display.fillRect(186, 55, 12, 3, GxEPD_BLACK);
    display.fillRect(195, 50, 3, 5, GxEPD_BLACK);

    display.fillRect(0, 0, 3, 12, GxEPD_BLACK);
    display.fillRect(0, 0, 12, 3, GxEPD_BLACK);

    display.fillRect(186, 0, 12, 3, GxEPD_BLACK);
    display.fillRect(195, 0, 3, 12, GxEPD_BLACK);

    // print date

    display.setTextSize(3);
    display.setCursor(17, 70);
    display.println(&timeinfo, "%a");
    display.println(&timeinfo, "%d");
    display.print(&timeinfo, "%Y");

    display.setCursor(35, 95);
    display.setTextSize(2);
    display.print(&timeinfo, "%b");

    display.setCursor(75, 80);
    display.setTextSize(1);
    display.print("week");
    display.fillTriangle(80, 75, 82, 78, 84, 75, GxEPD_BLACK);
    display.fillTriangle(80, 90, 82, 87, 84, 90, GxEPD_BLACK);
    display.drawLine(82, 75, 82, 65, GxEPD_BLACK);
    display.drawLine(82, 65, 20, 65, GxEPD_BLACK);

    display.drawLine(82, 87, 82, 100, GxEPD_BLACK);

    display.setCursor(72, 105);
    display.print("month");
    display.drawLine(70, 100, 100, 100, GxEPD_BLACK);
    display.drawLine(70, 115, 100, 115, GxEPD_BLACK);

    display.setCursor(75, 127);
    display.print("year");
    display.drawLine(82, 122, 82, 115, GxEPD_BLACK);
    display.fillTriangle(80, 122, 82, 125, 84, 122, GxEPD_BLACK);
    display.fillTriangle(80, 137, 82, 134, 84, 137, GxEPD_BLACK);
    display.drawLine(82, 137, 82, 147, GxEPD_BLACK);
    display.drawLine(82, 147, 0, 147, GxEPD_BLACK);

    // display.fillTriangle(60, 100, 70, 100, 65, 95, GxEPD_BLACK);

    // print planet and sunrise/sunset info

    display.drawBitmap(125, 68, planet, 75, 75, GxEPD_BLACK);
    display.fillRect(105, 75, 20, 3, GxEPD_BLACK);
    display.fillRect(105, 135, 20, 3, GxEPD_BLACK);

    display.setTextSize(1);
    display.setCursor(105, 65);
    display.printf("%02d", localWeather.sunrise.tm_hour);
    display.println("|DAWN");
    for (int i = 0; i <= 17; i++)
    {
        display.fillRect(105, 80 + (i * 3), 10, 2, GxEPD_BLACK);
    }
    display.setCursor(105, 140);
    display.printf("%02d", localWeather.sunset.tm_hour);
    display.print("|DASK");

    // print weather
    if (!localWeather.weatherDataQueue.empty())
    {

        display.setTextSize(3);
        display.setCursor(0, 150);
        display.print(localWeather.weatherDataQueue.front().temp);
        localWeather.weatherDataQueue.pop();
    }

    // display.fillTriangle(60, 180, 70, 185, 70, 175, GxEPD_BLACK);
    // display.setCursor(0, 180);
    // display.setTextSize(2);
    // display.print("temp");

    // print power
    display.drawRect(130, 150, 50, 20, GxEPD_BLACK);
    display.fillRect(130, 150, 45, 20, GxEPD_BLACK);

    // update display
    display.display();
}

void initDisplay()
{
    hspi.begin(SPI_SCK, -1, SPI_DIN, EPD_CS);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

    display.init(0, true);
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.display();
    display.setRotation(2);
}

void hibernateDisplay()
{
    display.hibernate();
}
void displayMenu()
{
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setTextSize(2);
    display.setCursor(0, 5);
    display.println("Prv + Upd Time");

    display.fillRect(0, 25, 150, 3, GxEPD_BLACK);

    display.setCursor(0, 35);
    display.println("Update Time");

    display.setCursor(0, 65);
    display.println("Weather Data");

    display.setCursor(0, 95);
    display.println("Back");
    display.display();
}

void updateMenu(int menuNumber)
{
    display.setPartialWindow(0, 0, 200, 200);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(2);
    int16_t lineHeigh = 30;
    if (menuNumber == 0)
        lineHeigh = 25;
    if (menuNumber == 1)
        lineHeigh = 55;
    if (menuNumber == 2)
        lineHeigh = 85;
    if (menuNumber == 3)
        lineHeigh = 115;

    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);

        display.setCursor(0, 5);
        display.println("Prv + Upd Time");

        display.setCursor(0, 35);
        display.println("Update Time");

        display.setCursor(0, 65);
        display.println("Weather Data");

        display.setCursor(0, 95);
        display.println("Back");

        display.fillRect(0, lineHeigh, 150, 3, GxEPD_BLACK);

    } while (display.nextPage());
}