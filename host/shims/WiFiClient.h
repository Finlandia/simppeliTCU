// WiFiClient backed by a plain POSIX TCP socket (uses the PC's network stack).
#pragma once

#include "Client.h"

class WiFiClient : public Client {
public:
    using Print::write;
    WiFiClient() = default;
    ~WiFiClient() override { stop(); }
    WiFiClient(const WiFiClient&) = delete;
    WiFiClient& operator=(const WiFiClient&) = delete;

    int connect(IPAddress ip, uint16_t port) override;
    int connect(const char* host, uint16_t port) override;
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* buf, size_t size) override;
    int available() override;
    int read() override;
    int read(uint8_t* buf, size_t size) override;
    int peek() override;
    void flush() override {}
    void stop() override;
    uint8_t connected() override;
    explicit operator bool() override { return fd_ >= 0; }

protected:
    int fd_ = -1;
};
