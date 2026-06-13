// temperature.cpp – DS18B20 Temperaturmessung

#include "temperature.h"
#include "pins.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire oneWire(DS18B20_PIN);
static DallasTemperature sensors(&oneWire);

void temperature_init() {
    sensors.begin();
    sensors.setResolution(12);
}

float temperature_read() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    if (temp == DEVICE_DISCONNECTED_C) {
        return -127.0f;
    }
    return temp;
}
