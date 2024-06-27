#include <GxEPD2_BW.h>
#include "bitmaps/Bitmaps152x152.h" // 1.54" b/w

#include "HttpEspWatch.h"
#include "IMG.h"

#ifndef PRINTESPWATCH_H
#define PRINTESPWATCH_H

#define SPI_SCK 14
#define SPI_DIN 13
#define EPD_CS 15
#define EPD_DC 2
#define SRAM_CS -1
#define EPD_RESET 17
#define EPD_BUSY 16

void printHour(uint16_t x, uint16_t y);
void printMinute(uint16_t x, uint16_t y);
void printDate(uint16_t x, uint16_t y);
void printPower(int voltage, uint16_t x, uint16_t y);
void printWeather(uint16_t x, uint16_t y);
void initDisplayText();
void initDisplay();
void hibernateDisplay();

#endif /* PRINTESPWATCH_H */