// WiFi stub: the host is always "connected" because the PC already has a network.
#pragma once

#include <cstdint>
#include <cstring>

#include "IPAddress.h"
#include "WiFiClient.h"

typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
} wl_status_t;

typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;

class WiFiClass {
public:
    bool mode(wifi_mode_t) { return true; }
    bool softAP(const char*, const char* = nullptr) { return true; }
    wl_status_t begin(const char*, const char* = nullptr) { return WL_CONNECTED; }
    bool setAutoReconnect(bool) { return true; }
    bool setSleep(bool) { return true; }
    wl_status_t status() { return WL_CONNECTED; }
    IPAddress localIP() { return IPAddress(127, 0, 0, 1); }
    IPAddress softAPIP() { return IPAddress(127, 0, 0, 1); }
    // Fixed, locally-administered fake MAC (matches ESP32 WiFi.macAddress(uint8_t*) signature).
    uint8_t* macAddress(uint8_t* mac) {
        static const uint8_t fake[6] = {0x02, 0x00, 0x00, 0x12, 0x34, 0x56};
        memcpy(mac, fake, sizeof(fake));
        return mac;
    }
};

inline WiFiClass WiFi;
