// knx_client.h – KNXnet/IP Kommunikation (Multicast Routing)

#pragma once

bool knx_init();
bool knx_send_temperature(const char* ga, float value);
