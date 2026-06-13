// main.cpp – Pool-Thermometer Hauptprogramm
//
// Betriebsmodi:
//   A) Dauerbetrieb (DEEP_SLEEP_ENABLED = false):
//      - WLAN dauerhaft verbunden
//      - MQTT dauerhaft verbunden (empfängt VL/RL live)
//      - Temperatur + KNX alle MEASURE_INTERVAL_SEC Sekunden
//      - Display-Update alle DISPLAY_INTERVAL_SEC Sekunden
//      - Webserver für OTA, Konfiguration, Live-Werte
//   B) Deep-Sleep-Betrieb (DEEP_SLEEP_ENABLED = true):
//      - Einmal messen, senden, anzeigen → schlafen

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

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
#include <esp_sleep.h>

// ---- Betriebsparameter ----
static bool deepSleepEnabled = false;          // false = Dauerbetrieb, true = Deep Sleep
static unsigned int deepSleepMinutes = DEEP_SLEEP_MINUTES;  // einstellbar via Web
static const unsigned long BOOT_GRACE_SEC = 180;         // 3 Min nach Boot: kein Deep Sleep
static const unsigned long MEASURE_INTERVAL_SEC = 60;    // Temperatur messen + MQTT/KNX senden
static const unsigned long DISPLAY_INTERVAL_SEC = 300;   // ePaper aktualisieren (5 Min)
static const unsigned long MQTT_LOOP_INTERVAL_MS = 100;  // MQTT client.loop()

// ---- Globale Messwerte ----
static float poolTemp = -127.0f;
static float battV = -1.0f;
static int   battPct = -1;
static unsigned long lastMeasure = 0;
static unsigned long lastDisplay = 0;
static unsigned long lastMqttLoop = 0;
static unsigned long bootTime = 0;

// ---- Webserver ----
static WebServer server(80);

// ---- Vorwärtsdeklarationen ----
static void web_setup();
static void do_measure_and_send();
static void do_display_update();
static void enter_deep_sleep();

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
    delay(3000);  // USB-CDC braucht Zeit zur Enumeration
    Serial.println("\n=== Pool-Thermometer Start ===");
    bootTime = millis();

    // Erkennung: erster Boot oder Deep Sleep Wakeup?
    bool isWakeup = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    Serial.printf("Boot-Grund: %s\n", isWakeup ? "Deep Sleep Wakeup" : "Power-On / Reset");

    // Hardware initialisieren
    temperature_init();
    battery_init();
    display_init();

    // Sofort DS18B20 messen
    poolTemp = temperature_read();
    battV = battery_read_voltage();
    battPct = battery_read_percent();
    Serial.printf("Erste Messung: Pool=%.1f°C\n", poolTemp);

    // Beim ersten Boot: Display sofort mit Pool-Temp anzeigen (ohne VL/RL)
    // Beim Wakeup: Display-Update erst nach MQTT VL/RL Empfang (nur 1 Refresh)
    if (!isWakeup) {
        display_update(poolTemp, -127.0f, -127.0f, battV, battPct);
        Serial.println("Display: Erste Anzeige (ohne VL/RL)");
    }

    // WLAN verbinden
    if (!wifi_connect(10000)) {
        if (isWakeup) {
            // Beim Wakeup noch kein Display-Update erfolgt → jetzt mit Pool-Temp
            display_update(poolTemp, -127.0f, -127.0f, battV, battPct);
        }
        if (deepSleepEnabled) {
            enter_deep_sleep();
        }
        return;
    }

    // MQTT verbinden und auf VL/RL warten
    mqtt_connect();
    mqtt_subscribe();
    Serial.println("MQTT: Warte auf VL/RL...");
    {
        bool gotVLRL = mqtt_wait_for_retained(20000);
        if (gotVLRL) {
            Serial.printf("VL/RL empfangen: VL=%.1f, RL=%.1f\n", mqtt_get_vl_temp(), mqtt_get_rl_temp());
        } else {
            Serial.println("MQTT: Timeout - VL/RL nicht empfangen");
        }
    }

    // Display mit vollständigen Werten aktualisieren
    display_update(poolTemp, mqtt_get_vl_temp(), mqtt_get_rl_temp(), battV, battPct);
    Serial.println("Display: Aktualisiert mit VL/RL");

    // KNX initialisieren und senden
    knx_init();
    if (poolTemp > -126.0f) {
        mqtt_publish_pool_temp(poolTemp);
        knx_send_temperature(KNX_GA_POOL_TEMP, poolTemp);
    }
    if (battV > 0) {
        mqtt_publish_battery(battV);
        knx_send_temperature(KNX_GA_BATTERY, battV);
    }

    // Immer Webserver starten (Boot Grace Period erlaubt Zugriff vor Deep Sleep)
    web_setup();
    Serial.printf("Webserver gestartet: http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.printf("Boot Grace Period: %lus (Deep Sleep verzögert)\n", BOOT_GRACE_SEC);
}

static void enter_deep_sleep() {
    Serial.printf("Deep Sleep: %d Minuten\n", deepSleepMinutes);
    mqtt_disconnect();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)deepSleepMinutes * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    unsigned long now = millis();
    unsigned long uptimeSec = (now - bootTime) / 1000;

    // Deep Sleep nach Grace Period
    if (deepSleepEnabled && uptimeSec >= BOOT_GRACE_SEC) {
        Serial.println("Boot Grace Period abgelaufen → Deep Sleep");
        enter_deep_sleep();
        return;  // wird nie erreicht
    }

    // MQTT keepalive
    if (now - lastMqttLoop >= MQTT_LOOP_INTERVAL_MS) {
        mqtt_loop();
        lastMqttLoop = now;
    }

    // Messen + Senden im Intervall
    if (now - lastMeasure >= MEASURE_INTERVAL_SEC * 1000UL) {
        do_measure_and_send();
        lastMeasure = now;
    }

    // Display im Intervall aktualisieren
    if (now - lastDisplay >= DISPLAY_INTERVAL_SEC * 1000UL) {
        do_display_update();
        lastDisplay = now;
    }

    // Webserver
    server.handleClient();

    // WLAN Reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WLAN: Verbindung verloren, reconnect...");
        wifi_connect(10000);
        if (WiFi.status() == WL_CONNECTED) {
            mqtt_connect();
            mqtt_subscribe();
        }
    }
}

// ---- Messen + Senden ----
static void do_measure_and_send() {
    poolTemp = temperature_read();
    battV = battery_read_voltage();
    battPct = battery_read_percent();

    Serial.printf("[%lus] Pool: %.1f°C", millis() / 1000, poolTemp);
    if (battV > 0) Serial.printf(" | Batt: %.2fV (%d%%)", battV, battPct);
    Serial.println();

    // MQTT publizieren
    if (poolTemp > -126.0f) mqtt_publish_pool_temp(poolTemp);
    if (battV > 0) mqtt_publish_battery(battV);

    // KNX senden
    if (poolTemp > -126.0f) knx_send_temperature(KNX_GA_POOL_TEMP, poolTemp);
    if (battV > 0) knx_send_temperature(KNX_GA_BATTERY, battV);
}

// ---- Display aktualisieren ----
static void do_display_update() {
    display_update(poolTemp, mqtt_get_vl_temp(), mqtt_get_rl_temp(), battV, battPct);
    Serial.println("Display aktualisiert");
}

// ---- Webserver ----
static const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pool-Thermometer</title>
<style>
body{font-family:sans-serif;margin:20px;background:#f5f5f5}
.card{background:#fff;border-radius:8px;padding:16px;margin:10px 0;box-shadow:0 2px 4px rgba(0,0,0,.1)}
h1{color:#333;font-size:1.4em} h2{color:#555;font-size:1.1em;margin:0 0 8px}
.val{font-size:2em;font-weight:bold;color:#1a73e8}
.small{color:#888;font-size:0.85em}
table{width:100%;border-collapse:collapse}
td{padding:6px 8px;border-bottom:1px solid #eee}
td:first-child{font-weight:bold;width:40%}
.btn{display:inline-block;padding:10px 20px;background:#1a73e8;color:#fff;border:none;border-radius:4px;cursor:pointer;text-decoration:none;margin:4px}
.btn:hover{background:#1557b0}
.btn-red{background:#d93025}
.btn-red:hover{background:#a50e0e}
</style>
</head><body>
)rawliteral";

static void handle_root() {
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Pool-Thermometer</h1>";

    // Messwerte
    html += "<div class='card'><h2>Aktuelle Werte</h2><table>";
    html += "<tr><td>Pooltemperatur</td><td class='val'>" + String(poolTemp, 1) + " &deg;C</td></tr>";
    html += "<tr><td>Vorlauf (VL)</td><td>" + String(mqtt_get_vl_temp(), 1) + " &deg;C</td></tr>";
    html += "<tr><td>R&uuml;cklauf (RL)</td><td>" + String(mqtt_get_rl_temp(), 1) + " &deg;C</td></tr>";
    if (battV > 0) {
        html += "<tr><td>Batterie</td><td>" + String(battV, 2) + " V (" + String(battPct) + "%)</td></tr>";
    }
    html += "</table></div>";

    // Status
    html += "<div class='card'><h2>Status</h2><table>";
    html += "<tr><td>WLAN</td><td>" + WiFi.localIP().toString() + " (" + String(WiFi.RSSI()) + " dBm)</td></tr>";
    html += "<tr><td>MQTT</td><td>" + String(mqtt_is_connected() ? "Verbunden" : "Getrennt") + "</td></tr>";
    html += "<tr><td>Uptime</td><td>" + String((millis() - bootTime) / 1000) + " s</td></tr>";
    html += "<tr><td>Messintervall</td><td>" + String(MEASURE_INTERVAL_SEC) + " s</td></tr>";
    html += "<tr><td>Display-Intervall</td><td>" + String(DISPLAY_INTERVAL_SEC) + " s</td></tr>";
    html += "<tr><td>Deep Sleep</td><td>" + String(deepSleepEnabled ? "Aktiviert" : "Deaktiviert") + "</td></tr>";
    html += "<tr><td>Sleep-Dauer</td><td>" + String(deepSleepMinutes) + " Min</td></tr>";
    if (deepSleepEnabled) {
        unsigned long remaining = 0;
        unsigned long uptimeSec = (millis() - bootTime) / 1000;
        if (uptimeSec < BOOT_GRACE_SEC) remaining = BOOT_GRACE_SEC - uptimeSec;
        html += "<tr><td>Grace Period</td><td>" + String(remaining) + " s verbleibend</td></tr>";
    }
    html += "</table></div>";

    // Aktionen
    html += "<div class='card'><h2>Aktionen</h2>";
    html += "<a class='btn' href='/measure'>Jetzt messen</a>";
    html += "<a class='btn' href='/display'>Display aktualisieren</a>";
    if (deepSleepEnabled) {
        html += "<a class='btn' href='/deepsleep?enable=0'>Deep Sleep AUS</a>";
        html += "<a class='btn btn-red' href='/sleep'>Sofort schlafen</a>";
    } else {
        html += "<a class='btn btn-red' href='/deepsleep?enable=1'>Deep Sleep EIN</a>";
    }
    html += "<a class='btn' href='/ota'>OTA Update</a>";
    html += "</div>";

    // Sleep-Dauer einstellen
    html += "<div class='card'><h2>Sleep-Dauer</h2>";
    html += "<form action='/setsleep' method='GET'>";
    html += "<input type='number' name='min' value='" + String(deepSleepMinutes) + "' min='1' max='60' style='width:60px'> Minuten ";
    html += "<input type='submit' value='Speichern' class='btn'>";
    html += "</form></div>";

    html += "<p class='small'>Pool-Thermometer XIAO ESP32-C3 | Uptime: " + String((millis() - bootTime) / 1000) + "s</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

static void handle_measure() {
    do_measure_and_send();
    server.sendHeader("Location", "/");
    server.send(302);
}

static void handle_display() {
    do_display_update();
    server.sendHeader("Location", "/");
    server.send(302);
}

static void handle_sleep() {
    server.send(200, "text/html", "<html><body><h1>Deep Sleep wird aktiviert...</h1><p>Ger&auml;t geht in 2 Sekunden schlafen.</p></body></html>");
    delay(2000);
    enter_deep_sleep();
}

static void handle_deepsleep_toggle() {
    if (server.hasArg("enable")) {
        deepSleepEnabled = (server.arg("enable") == "1");
        Serial.printf("Deep Sleep %s\n", deepSleepEnabled ? "aktiviert" : "deaktiviert");
    }
    server.sendHeader("Location", "/");
    server.send(302);
}

static void handle_set_sleep_duration() {
    if (server.hasArg("min")) {
        int val = server.arg("min").toInt();
        if (val >= 1 && val <= 60) {
            deepSleepMinutes = val;
            Serial.printf("Sleep-Dauer: %d Min\n", deepSleepMinutes);
        }
    }
    server.sendHeader("Location", "/");
    server.send(302);
}

static void handle_ota_page() {
    String html = FPSTR(HTML_HEADER);
    html += "<h1>OTA Firmware Update</h1>";
    html += "<div class='card'>";
    html += "<form method='POST' action='/ota' enctype='multipart/form-data'>";
    html += "<p>Firmware-Datei (.bin) ausw&auml;hlen:</p>";
    html += "<input type='file' name='firmware' accept='.bin'><br><br>";
    html += "<input type='submit' value='Upload &amp; Flash' class='btn'>";
    html += "</form></div>";
    html += "<p class='small'>Datei: .pio/build/seeed_xiao_esp32c3/firmware.bin</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

static void handle_ota_upload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA: Start %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA: Erfolg, %u Bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

static void handle_ota_result() {
    if (Update.hasError()) {
        server.send(500, "text/html", "<h1>OTA Fehler!</h1><a href='/ota'>Zur&uuml;ck</a>");
    } else {
        server.send(200, "text/html", "<h1>OTA Erfolg!</h1><p>Neustart in 2 Sekunden...</p>");
        delay(2000);
        ESP.restart();
    }
}

static void web_setup() {
    server.on("/", handle_root);
    server.on("/measure", handle_measure);
    server.on("/display", handle_display);
    server.on("/sleep", handle_sleep);
    server.on("/deepsleep", handle_deepsleep_toggle);
    server.on("/setsleep", handle_set_sleep_duration);
    server.on("/ota", HTTP_GET, handle_ota_page);
    server.on("/ota", HTTP_POST, handle_ota_result, handle_ota_upload);
    server.begin();
}
