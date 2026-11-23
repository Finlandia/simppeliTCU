// Minimal Arduino replacement for unit tests: only what canInterface.cpp uses.
// Time and Serial are fully controlled by the test (see fake_arduino.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// millis() returns this value; tests set it directly.
extern unsigned long fakeMillis;
unsigned long millis();

// Serial that captures output in a string and reports a configurable free TX buffer size.
class FakeSerial {
public:
    std::string output;
    int writeCapacity = 4096;

    int availableForWrite() const { return writeCapacity; }

    size_t write(const uint8_t* buf, size_t size) {
        output.append(reinterpret_cast<const char*>(buf), size);
        return size;
    }

    size_t println(const char* s) {
        size_t n = strlen(s);
        output.append(s, n);
        output.append("\r\n");
        return n + 2;
    }

    void reset() {
        output.clear();
        writeCapacity = 4096;
    }
};

extern FakeSerial Serial;
