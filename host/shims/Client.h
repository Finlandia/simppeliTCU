#pragma once

#include "Stream.h"
#include "IPAddress.h"

class Client : public Stream {
public:
    using Print::write;
    virtual int connect(IPAddress ip, uint16_t port) = 0;
    virtual int connect(const char* host, uint16_t port) = 0;
    size_t write(uint8_t c) override = 0;
    size_t write(const uint8_t* buf, size_t size) override = 0;
    int available() override = 0;
    int read() override = 0;
    virtual int read(uint8_t* buf, size_t size) = 0;
    int peek() override = 0;
    void flush() override = 0;
    virtual void stop() = 0;
    virtual uint8_t connected() = 0;
    virtual explicit operator bool() = 0;
};
