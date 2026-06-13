// battery.cpp – Batteriespannung messen (XIAO ESP32-C3)

#include "battery.h"
#include "pins.h"
#include <Arduino.h>

// XIAO ESP32-C3 Batteriemessung:
// Interner Spannungsteiler, ADC 12-Bit (0–4095), Referenz ~2.5V
// Spannungsteiler-Faktor muss ggf. kalibriert werden
static const float VOLTAGE_DIVIDER_FACTOR = 2.0f;
static const float ADC_REF_VOLTAGE = 2.5f;
static const int   ADC_RESOLUTION = 4095;

void battery_init() {
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);
}

float battery_read_voltage() {
    int raw = analogRead(BATTERY_ADC_PIN);
    float voltage = (raw / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_FACTOR;
    return voltage;
}

int battery_read_percent() {
    float voltage = battery_read_voltage();
    // LiPo: 4.2V = 100%, 3.0V = 0%
    int percent = (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f);
    if (percent > 100) percent = 100;
    if (percent < 0)   percent = 0;
    return percent;
}
