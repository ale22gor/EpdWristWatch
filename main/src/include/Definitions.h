#ifndef DEFINITIONS_H
#define DEFINITIONS_H


#define WEATHERTIMESTUMPS 40

struct weatherData
{
  u16_t weather;
  int8_t temp;
  struct tm time;
};


#endif /* DEFINITIONS_H */