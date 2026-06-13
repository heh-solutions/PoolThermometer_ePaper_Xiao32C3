// knx_client.cpp – KNXnet/IP Kommunikation (UDP Tunneling)
//
// Sendet Temperaturwerte als DPT 9.001 (2-Byte Float) an das KNX-IP-Gateway.
// TODO: Vollständige KNXnet/IP Tunneling-Implementierung oder esp-knx-ip Library.
//       Aktuell Platzhalter – wird nach Library-Evaluierung implementiert.

#include "knx_client.h"
#include <Arduino.h>
#include <WiFiUdp.h>

#if __has_include("parameter.h")
  #include "parameter.h"
#else
  #define KNX_GATEWAY_IP   ""
  #define KNX_GATEWAY_PORT 3671
  #define KNX_GA_POOL_TEMP "10/3/62"
  #define KNX_GA_BATTERY   "10/3/63"
#endif

// DPT 9.001 Encoding: 2-Byte Float (MEEEEMMM MMMMMMMM)
static void encode_dpt9(float value, uint8_t* out) {
    int sign = 0;
    int mantissa;
    int exponent = 0;

    if (value < 0) {
        sign = 1;
        value = -value;
    }

    mantissa = (int)(value * 100.0f);

    while (mantissa > 2047) {
        mantissa >>= 1;
        exponent++;
    }

    if (sign) {
        mantissa = -mantissa;
        mantissa = mantissa & 0x07FF;
        mantissa |= 0x0800;
    }

    out[0] = (uint8_t)((sign << 7) | (exponent << 3) | ((mantissa >> 8) & 0x07));
    out[1] = (uint8_t)(mantissa & 0xFF);
}

bool knx_send_temperature(const char* ga, float value) {
    if (strlen(KNX_GATEWAY_IP) == 0) {
        Serial.println("KNX: Gateway-IP nicht konfiguriert");
        return false;
    }

    uint8_t dpt[2];
    encode_dpt9(value, dpt);

    Serial.printf("KNX: Sende %.1f an %s (DPT9: 0x%02X%02X)\n", value, ga, dpt[0], dpt[1]);

    // TODO: KNXnet/IP Tunneling Connection Request / Data Request implementieren
    // Oder esp-knx-ip Library einbinden nach Evaluierung
    Serial.println("KNX: STUB – KNXnet/IP Tunneling noch nicht implementiert");

    return false;
}
