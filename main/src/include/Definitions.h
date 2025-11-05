#ifndef DEFINITIONS_H
#define DEFINITIONS_H


#define WEATHERTIMESTUMPS 40

struct weatherData
{
  u16_t weather;
  int8_t temp;
  struct tm time;
};


#define PIN_MOTOR 4
#define PIN_KEY 35
#define PWR_EN 5
#define Backlight 33
#define Bat_ADC 34

#endif /* DEFINITIONS_H */