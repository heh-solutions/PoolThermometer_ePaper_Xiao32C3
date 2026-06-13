// knx_client.cpp – KNXnet/IP Multicast Routing
//
// Sendet Temperaturwerte als DPT 9.001 per KNXnet/IP Routing Indication
// an die KNX-Multicast-Adresse 224.0.23.12:3671.
// Das KNX-IP-Gateway leitet die Telegramme auf den physischen Bus weiter.

#include "knx_client.h"
#include <Arduino.h>
#include <WiFiUdp.h>

#if __has_include("parameter.h")
  #include "parameter.h"
#else
  #define KNX_GATEWAY_IP   ""
  #define KNX_GATEWAY_PORT 3671
  #define KNX_PHYS_ADDR    "1.1.165"
  #define KNX_GA_POOL_TEMP "10/3/62"
  #define KNX_GA_BATTERY   "10/3/63"
#endif

// KNXnet/IP Multicast
static const IPAddress KNX_MULTICAST_IP(224, 0, 23, 12);
static const uint16_t KNX_MULTICAST_PORT = 3671;

static WiFiUDP knxUdp;
static bool knxInitialized = false;

// Physische Adresse parsen: "1.1.165" → 2 Bytes
static uint16_t parse_phys_addr(const char* addr) {
    int area = 0, line = 0, device = 0;
    sscanf(addr, "%d.%d.%d", &area, &line, &device);
    return (uint16_t)((area << 12) | (line << 8) | device);
}

// Gruppenadresse parsen: "10/3/62" → 2 Bytes
static uint16_t parse_group_addr(const char* ga) {
    int main_g = 0, middle = 0, sub = 0;
    sscanf(ga, "%d/%d/%d", &main_g, &middle, &sub);
    return (uint16_t)((main_g << 11) | (middle << 8) | sub);
}

// DPT 9.001 Encoding: 2-Byte Float (MEEEEMMM MMMMMMMM)
static void encode_dpt9(float value, uint8_t* out) {
    // Vorzeichen
    int sign = 0;
    if (value < 0) {
        sign = 1;
        value = -value;
    }

    int mantissa = (int)(value * 100.0f);
    int exponent = 0;

    while (mantissa > 2047) {
        mantissa >>= 1;
        exponent++;
    }

    if (sign) {
        mantissa = (~mantissa + 1) & 0x07FF;
    }

    out[0] = (uint8_t)((sign << 7) | (exponent << 3) | ((mantissa >> 8) & 0x07));
    out[1] = (uint8_t)(mantissa & 0xFF);
}

bool knx_init() {
    if (strlen(KNX_GATEWAY_IP) == 0) {
        Serial.println("KNX: Gateway-IP nicht konfiguriert");
        return false;
    }

    // Multicast-Gruppe beitreten
    if (knxUdp.beginMulticast(KNX_MULTICAST_IP, KNX_MULTICAST_PORT)) {
        knxInitialized = true;
        Serial.println("KNX: Multicast initialisiert (224.0.23.12:3671)");
        return true;
    }
    Serial.println("KNX: Multicast-Init fehlgeschlagen");
    return false;
}

bool knx_send_temperature(const char* ga, float value) {
    if (!knxInitialized) {
        if (!knx_init()) return false;
    }

    uint16_t srcAddr = parse_phys_addr(KNX_PHYS_ADDR);
    uint16_t dstAddr = parse_group_addr(ga);

    uint8_t dpt[2];
    encode_dpt9(value, dpt);

    // KNXnet/IP Routing Indication Frame bauen
    // Header (6 Bytes) + cEMI (11 + 2 = 13 Bytes) = 19 Bytes total
    uint8_t frame[19];

    // --- KNXnet/IP Header (6 Bytes) ---
    frame[0] = 0x06;        // Header Length
    frame[1] = 0x10;        // Protocol Version 1.0
    frame[2] = 0x05;        // Service Type: ROUTING_INDICATION (high)
    frame[3] = 0x30;        // Service Type: ROUTING_INDICATION (low)
    frame[4] = 0x00;        // Total Length (high)
    frame[5] = 19;          // Total Length (low) = 6 + 13

    // --- cEMI Frame (13 Bytes) ---
    frame[6]  = 0x29;       // Message Code: L_Data.ind
    frame[7]  = 0x00;       // Additional Info Length: 0
    frame[8]  = 0xBC;       // Ctrl1: Standard Frame, No Repeat, Broadcast, Prio Low, No Ack
    frame[9]  = 0xE0;       // Ctrl2: Destination = Group Address, Hop Count = 6
    frame[10] = (srcAddr >> 8) & 0xFF;  // Source Address (high)
    frame[11] = srcAddr & 0xFF;         // Source Address (low)
    frame[12] = (dstAddr >> 8) & 0xFF;  // Destination GA (high)
    frame[13] = dstAddr & 0xFF;         // Destination GA (low)
    frame[14] = 0x03;       // Data Length: 3 (APCI 2 Bytes + DPT 2 Bytes, aber Length = Payload-1)
    // APCI: GroupValueWrite (0x00 0x80) + Data
    frame[15] = 0x00;       // TPCI + APCI (high): numbered, GroupValueWrite
    frame[16] = 0x80;       // APCI (low): GroupValueWrite
    frame[17] = dpt[0];     // DPT 9.001 Byte 1
    frame[18] = dpt[1];     // DPT 9.001 Byte 2

    // Per UDP Multicast senden
    knxUdp.beginPacket(KNX_MULTICAST_IP, KNX_MULTICAST_PORT);
    knxUdp.write(frame, sizeof(frame));
    int result = knxUdp.endPacket();

    if (result) {
        Serial.printf("KNX: %.1f an %s gesendet (DPT9: 0x%02X%02X)\n", value, ga, dpt[0], dpt[1]);
        return true;
    }
    Serial.printf("KNX: Senden an %s fehlgeschlagen\n", ga);
    return false;
}

