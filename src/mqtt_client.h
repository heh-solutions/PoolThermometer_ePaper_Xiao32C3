// mqtt_client.h – MQTT Verbindung & Topics

#pragma once

bool mqtt_connect();
void mqtt_subscribe();
bool mqtt_wait_for_retained(unsigned long timeout_ms);
void mqtt_publish_pool_temp(float temp);
void mqtt_publish_battery(float voltage);
float mqtt_get_vl_temp();
float mqtt_get_rl_temp();
void mqtt_disconnect();
