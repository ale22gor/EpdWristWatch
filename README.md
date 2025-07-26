# EpdWristWatch

### components/Adafruit_BusIO/CMakeLists.txt
```
# Adafruit Bus IO Library
# https://github.com/adafruit/Adafruit_BusIO
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_I2CDevice.cpp" "Adafruit_BusIO_Register.cpp" "Adafruit_SPIDevice.cpp" 
                       INCLUDE_DIRS "."
                       REQUIRES arduino)
project(Adafruit_BusIO)
```

### components/Adafruit-GFX-Library/CMakeLists.txt

```
# Adafruit GFX Library
# https://github.com/adafruit/Adafruit-GFX-Library
# BSD License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_GFX.cpp" "Adafruit_GrayOLED.cpp" "Adafruit_SPITFT.cpp" "glcdfont.c"
                       INCLUDE_DIRS "."
                       REQUIRES arduino Adafruit_BusIO)
project(Adafruit-GFX-Library)
```

### components/GxEPD2/CMakeLists.txt
```
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/components/GxEPD2/src/*.cpp)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "src"
                       REQUIRES arduino Adafruit-GFX-Library Adafruit_BusIO)
```