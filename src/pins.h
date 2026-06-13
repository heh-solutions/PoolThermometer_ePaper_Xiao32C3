// pins.h – Pin-Definitionen für XIAO ESP32-C3

#pragma once

// ---- ePaper Display (SPI) ----
#define EPAPER_BUSY   6   // GPIO6
#define EPAPER_RES    5   // GPIO5
#define EPAPER_DC     4   // GPIO4
#define EPAPER_CS     3   // GPIO3
#define EPAPER_SCK    8   // GPIO8
#define EPAPER_MOSI   10  // GPIO10

// ---- DS18B20 Temperatursensor ----
#define DS18B20_PIN   2   // GPIO2 (4.7k Pullup nach 3.3V)

// ---- Batterie ADC ----
// XIAO ESP32-C3: Batteriespannung über internen Spannungsteiler messbar
#define BATTERY_ADC_PIN A0
