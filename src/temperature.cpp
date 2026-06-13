// temperature.cpp – DS18B20 Temperaturmessung

#include "temperature.h"
#include "pins.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire oneWire(DS18B20_PIN);
static DallasTemperature sensors(&oneWire);

void temperature_init() {
    sensors.begin();
    
    int deviceCount = sensors.getDeviceCount();
    Serial.printf("DS18B20: %d Sensor(en) auf GPIO%d gefunden\n", deviceCount, DS18B20_PIN);
    
    if (deviceCount > 0) {
        sensors.setResolution(12);
        DeviceAddress addr;
        if (sensors.getAddress(addr, 0)) {
            Serial.printf("DS18B20: Adresse = %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                          addr[0], addr[1], addr[2], addr[3],
                          addr[4], addr[5], addr[6], addr[7]);
        }
    } else {
        Serial.println("DS18B20: KEIN Sensor erkannt! Prüfe:");
        Serial.println("  - Verkabelung: DATA auf GPIO2, 4.7k Pullup nach 3.3V");
        Serial.println("  - Sensor-Typ: Gelb=DATA, Rot=VCC(3.3V), Schwarz=GND");
    }
}

float temperature_read() {
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);
    if (temp == DEVICE_DISCONNECTED_C) {
        Serial.println("DS18B20: Sensor nicht erreichbar (-127)");
        return -127.0f;
    }
    return temp;
}
