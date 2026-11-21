// TLS is not implemented in the host build. Connecting prints a hint and fails,
// so configure the host instance with `set mqtt_tls false` and a plain MQTT port.
#pragma once

#include "WiFiClient.h"

class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
    void setCACert(const char*) {}
    int connect(IPAddress ip, uint16_t port) override { return connect(ip.toString().c_str(), port); }
    int connect(const char* host, uint16_t port) override;
};
