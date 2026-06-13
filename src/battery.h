// battery.h – Batteriespannung messen

#pragma once

void battery_init();
float battery_read_voltage();
int battery_read_percent();
