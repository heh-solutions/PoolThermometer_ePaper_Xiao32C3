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
// ACHTUNG: GPIO7 hat KEINEN ADC am ESP32-C3! ADC nur auf GPIO0-5.
// Lösung: Batterie-Spannungsteiler auf GPIO3 (D1/A1) umverdrahten,
//         ePaper CS von GPIO3 auf GPIO7 (D5) verschieben.
// Bis zur Neuverkabelung: BATTERY_HW_PRESENT = false in battery.cpp
#define BATTERY_ADC_PIN    3   // GPIO3 (A1, ADC1_CH3) – NACH Umverdrahtung!
#define BATTERY_ENABLE_PIN 9   // GPIO9 (P-MOSFET Gate, Phase 2)
