#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

class String;
class Print;

class Printable {
public:
    virtual ~Printable() = default;
    virtual size_t printTo(Print& p) const = 0;
};

class Print {
public:
    virtual ~Print() = default;

    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t* buf, size_t size) {
        size_t n = 0;
        while (size--) n += write(*buf++);
        return n;
    }
    size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
    size_t write(const char* buf, size_t size) { return write((const uint8_t*)buf, size); }
    virtual int availableForWrite() { return 0; }
    virtual void flush() {}

    size_t print(const char* s) { return write(s); }
    size_t print(const String& s);
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(unsigned char v, int base = DEC_BASE) { return print((unsigned long)v, base); }
    size_t print(int v, int base = DEC_BASE) { return print((long)v, base); }
    size_t print(unsigned int v, int base = DEC_BASE) { return print((unsigned long)v, base); }
    size_t print(long v, int base = DEC_BASE);
    size_t print(unsigned long v, int base = DEC_BASE);
    size_t print(long long v, int base = DEC_BASE) { return print((long)v, base); }
    size_t print(unsigned long long v, int base = DEC_BASE) { return print((unsigned long)v, base); }
    size_t print(double v, int digits = 2);
    size_t print(const Printable& p) { return p.printTo(*this); }

    size_t println() { return write("\r\n"); }
    template <typename T>
    size_t println(const T& v) { size_t n = print(v); return n + println(); }
    template <typename T>
    size_t println(const T& v, int arg) { size_t n = print(v, arg); return n + println(); }

    size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

private:
    static constexpr int DEC_BASE = 10;
};
