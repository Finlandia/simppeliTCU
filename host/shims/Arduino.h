// Minimal Arduino core replacement for building simppeliTCU as a Linux program.
// Only the parts of the Arduino / ESP32 API used by the firmware (and PubSubClient)
// are implemented.
#pragma once

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef uint8_t byte;
typedef bool boolean;

#define F(x) (x)
#define PROGMEM
#define PSTR(x) (x)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_byte_near(p) (*(const uint8_t*)(p))

#define DEC 10
#define HEX 16

#define LOW 0
#define HIGH 1
#define INPUT 0
#define OUTPUT 1

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void yield();
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return LOW; }

#include "WString.h"
#include "Print.h"
#include "Stream.h"

// Serial maps to stdout (output) and stdin (input, non-blocking).
class HardwareSerial : public Stream {
public:
    using Print::write;
    void begin(unsigned long) {}
    void end() {}
    size_t setTxBufferSize(size_t n) { return n; }
    size_t setRxBufferSize(size_t n) { return n; }
    int available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buf, size_t size) override;
    int availableForWrite() override { return 4096; }
    void flush() override;
    explicit operator bool() const { return true; }
};
extern HardwareSerial Serial;

class EspClass {
public:
    [[noreturn]] void restart();
};
extern EspClass ESP;
