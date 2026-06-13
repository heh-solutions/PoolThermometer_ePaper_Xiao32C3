// battery.cpp – Batteriespannung messen (XIAO ESP32-C3)
//
// Phase 1: Einfacher Spannungsteiler 2x 1MΩ (Dauerverlust ~2µA, akzeptabel)
// Phase 2 (später): P-MOSFET (Si2301) zum Abschalten des Teilers im Deep Sleep
//                   → GPIO9 als Enable-Pin, 0µA im Sleep

#include "battery.h"
#include "pins.h"
#include <Arduino.h>

// Spannungsteiler: BAT+ → 1MΩ → GPIO7 → 1MΩ → GND
// Teilerverhältnis 1:2, Dauerstrom ~2µA (bei 4.2V)
static const float VOLTAGE_DIVIDER_FACTOR = 2.0f;   // 1M/1M Teiler
static const float ADC_REF_VOLTAGE = 2.5f;           // ESP32-C3 ADC max bei 11dB
static const int   ADC_RESOLUTION = 4095;

// Feature-Flags
static const bool BATTERY_HW_PRESENT = true;   // Phase 1: Spannungsteiler bestückt
static const bool BATTERY_MOSFET_CTRL = false;  // Phase 2: P-MOSFET Enable (GPIO9)

void battery_init() {
    if (!BATTERY_HW_PRESENT) return;
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);

    if (BATTERY_MOSFET_CTRL) {
        pinMode(BATTERY_ENABLE_PIN, OUTPUT);
        digitalWrite(BATTERY_ENABLE_PIN, HIGH);  // MOSFET aus (P-FET: HIGH=sperrt)
    }
}

float battery_read_voltage() {
    if (!BATTERY_HW_PRESENT) return -1.0f;

    if (BATTERY_MOSFET_CTRL) {
        digitalWrite(BATTERY_ENABLE_PIN, LOW);   // MOSFET ein
        delay(5);                                // Spannung stabilisieren
    }

    // Mehrere Messungen mitteln für stabiles Ergebnis
    long sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(BATTERY_ADC_PIN);
    }
    int raw = sum / 8;

    if (BATTERY_MOSFET_CTRL) {
        digitalWrite(BATTERY_ENABLE_PIN, HIGH);  // MOSFET aus → 0µA
    }

    float voltage = (raw / (float)ADC_RESOLUTION) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER_FACTOR;
    return voltage;
}

int battery_read_percent() {
    if (!BATTERY_HW_PRESENT) return -1;
    float voltage = battery_read_voltage();
    // LiPo: 4.2V = 100%, 3.0V = 0%
    int percent = (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f);
    if (percent > 100) percent = 100;
    if (percent < 0)   percent = 0;
    return percent;
}
