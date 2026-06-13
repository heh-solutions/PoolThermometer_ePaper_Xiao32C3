// mqtt_client.cpp – MQTT Verbindung & Topics

#include "mqtt_client.h"
#include <WiFi.h>
#include <PubSubClient.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "secrets.h nicht gefunden – bitte secrets.h.example kopieren und anpassen!"
  #define WIFI_SSID ""
  #define WIFI_PSK  ""
  #define MQTT_USER ""
  #define MQTT_PW   ""
#endif

#if __has_include("parameter.h")
  #include "parameter.h"
#else
  #warning "parameter.h nicht gefunden – bitte parameter.h.example kopieren und anpassen!"
  #define MQTT_BROKER     ""
  #define MQTT_PORT       18833
  #define MQTT_CLIENT     "PoolThermometer"
  #define TOPIC_VL_TEMP   "pool/heizung/vorlauf/state"
  #define TOPIC_RL_TEMP   "pool/heizung/ruecklauf/state"
  #define TOPIC_POOL_TEMP "pool/thermometer/pooltemperatur/state"
  #define TOPIC_BATTERY   "pool/thermometer/battery/state"
  #define TOPIC_STATUS    "pool/thermometer/status"
#endif

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static float vlTemp = -127.0f;
static float rlTemp = -127.0f;
static bool vlReceived = false;
static bool rlReceived = false;

static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    char msg[64];
    unsigned int copyLen = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
    memcpy(msg, payload, copyLen);
    msg[copyLen] = '\0';

    // DEBUG: Alle empfangenen Topics ausgeben
    Serial.printf("MQTT RX: [%s] = '%s'\n", topic, msg);

    if (strcmp(topic, TOPIC_VL_TEMP) == 0) {
        vlTemp = atof(msg);
        vlReceived = true;
    } else if (strcmp(topic, TOPIC_RL_TEMP) == 0) {
        rlTemp = atof(msg);
        rlReceived = true;
    }
}

bool mqtt_connect() {
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);

    Serial.printf("MQTT: Verbinde mit %s:%d...\n", MQTT_BROKER, MQTT_PORT);

    bool connected;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PW,
                                       TOPIC_STATUS, 0, true, "offline");
    } else {
        connected = mqttClient.connect(MQTT_CLIENT, nullptr, nullptr,
                                       TOPIC_STATUS, 0, true, "offline");
    }

    if (connected) {
        Serial.println("MQTT: Verbunden");
        mqttClient.publish(TOPIC_STATUS, "online", true);
    } else {
        Serial.printf("MQTT: Fehler, rc=%d\n", mqttClient.state());
    }
    return connected;
}

void mqtt_subscribe() {
    mqttClient.subscribe(TOPIC_VL_TEMP);
    mqttClient.subscribe(TOPIC_RL_TEMP);
    Serial.printf("MQTT: Abonniert: '%s', '%s'\n", TOPIC_VL_TEMP, TOPIC_RL_TEMP);
}

bool mqtt_wait_for_retained(unsigned long timeout_ms) {
    unsigned long start = millis();
    while ((!vlReceived || !rlReceived) && (millis() - start < timeout_ms)) {
        mqttClient.loop();
        delay(10);
    }
    return vlReceived && rlReceived;
}

void mqtt_publish_pool_temp(float temp) {
    char buf[16];
    dtostrf(temp, 1, 1, buf);
    mqttClient.publish(TOPIC_POOL_TEMP, buf, true);
    Serial.printf("MQTT: Pool %.1f°C publiziert\n", temp);
}

void mqtt_publish_battery(float voltage) {
    char buf[16];
    dtostrf(voltage, 1, 2, buf);
    mqttClient.publish(TOPIC_BATTERY, buf, true);
    Serial.printf("MQTT: Batterie %.2fV publiziert\n", voltage);
}

float mqtt_get_vl_temp() { return vlTemp; }
float mqtt_get_rl_temp() { return rlTemp; }

void mqtt_disconnect() {
    mqttClient.publish(TOPIC_STATUS, "sleeping", true);
    mqttClient.disconnect();
}

void mqtt_loop() {
    if (mqttClient.connected()) {
        mqttClient.loop();
    } else {
        // Auto-Reconnect
        if (mqtt_connect()) {
            mqtt_subscribe();
        }
    }
}

bool mqtt_is_connected() {
    return mqttClient.connected();
}
