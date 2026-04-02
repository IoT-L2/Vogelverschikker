// network.h
#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID      "iPhone van Christian"
#define WIFI_PASS      "#IoT12345"
#define WIFI_MAX_RETRY  5

    void network_init(void);
    bool network_is_connected(void);
    void network_send_node_data(uint8_t node_id, uint16_t total, const char* sound_name);

#ifdef __cplusplus
}
#endif