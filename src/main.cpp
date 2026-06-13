// main.cpp – Pool-Thermometer Hauptprogramm
//
// Ablauf pro Wakeup:
//   1. WLAN verbinden
//   2. MQTT verbinden + retained Messages empfangen (VL/RL)
//   3. DS18B20 messen (Pooltemperatur)
//   4. Batteriespannung messen
//   5. Pooltemperatur + Batterie per MQTT publizieren
//   6. Pooltemperatur + Batterie per KNXnet/IP senden
//   7. ePaper aktualisieren
//   8. Deep Sleep 5 Minuten

#include <Arduino.h>
#include <WiFi.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "secrets.h nicht gefunden – bitte secrets.h.example kopieren und anpassen!"
  #define WIFI_SSID ""
  #define WIFI_PSK  ""
#endif

#if __has_include("parameter.h")
  #include "parameter.h"
#else
  #warning "parameter.h nicht gefunden – bitte parameter.h.example kopieren und anpassen!"
  #define WIFI_HOSTNAME      "PoolThermometer"
  #define DEEP_SLEEP_MINUTES 5
#endif

#include "pins.h"
#include "temperature.h"
#include "battery.h"
#include "mqtt_client.h"
#include "display_view.h"
#include "knx_client.h"

static bool wifi_connect(unsigned long timeout_ms) {
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PSK);

    Serial.printf("WLAN: Verbinde mit %s", WIFI_SSID);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < timeout_ms)) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WLAN: Verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("WLAN: Verbindung fehlgeschlagen!");
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== Pool-Thermometer Start ===");

    // 1. Sensoren initialisieren (parallel zum WLAN-Verbindungsaufbau)
    temperature_init();
    battery_init();
    display_init();

    // 2. DS18B20 Messung starten (braucht ~750ms bei 12-Bit)
    float poolTemp = temperature_read();
    Serial.printf("Pool: %.1f°C\n", poolTemp);

    // 3. Batterie messen
    float battV = battery_read_voltage();
    int battPct = battery_read_percent();
    if (battV > 0) {
        Serial.printf("Batterie: %.2fV (%d%%)\n", battV, battPct);
    } else {
        Serial.println("Batterie: Messung nicht verfügbar");
    }

    // 4. WLAN verbinden
    if (!wifi_connect(10000)) {
        // Ohne WLAN: nur lokale Werte anzeigen
        display_update(poolTemp, -127.0f, -127.0f, battV, battPct);
        goto sleep;
    }

    // 5. MQTT verbinden und retained Messages empfangen
    if (mqtt_connect()) {
        mqtt_subscribe();
        mqtt_wait_for_retained(5000);

        // 6. Pooltemperatur + Batterie publizieren
        if (poolTemp > -126.0f) {
            mqtt_publish_pool_temp(poolTemp);
        }
        if (battV > 0) {
            mqtt_publish_battery(battV);
        }

        mqtt_disconnect();
    }

    // 7. KNXnet/IP senden
    knx_init();
    if (poolTemp > -126.0f) {
        knx_send_temperature(KNX_GA_POOL_TEMP, poolTemp);
    }
    if (battV > 0) {
        knx_send_temperature(KNX_GA_BATTERY, battV);
    }

    // 8. Display aktualisieren
    display_update(poolTemp, mqtt_get_vl_temp(), mqtt_get_rl_temp(), battV, battPct);

sleep:
    // 9. Deep Sleep
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.printf("Deep Sleep: %d Minuten\n", DEEP_SLEEP_MINUTES);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_MINUTES * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Bleibt leer – alles passiert in setup(), danach Deep Sleep
}
