#include "Arduino.h"

#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>

#include "host_config.h"

// ---------------------------------------------------------------- time / misc

static const auto startTime = std::chrono::steady_clock::now();

unsigned long millis() {
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count();
}

unsigned long micros() {
    return (unsigned long)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - startTime).count();
}

void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void yield() { std::this_thread::yield(); }

long random(long max) { return max > 0 ? (long)(::random() % max) : 0; }
long random(long min, long max) { return max > min ? min + random(max - min) : min; }
void randomSeed(unsigned long seed) { srandom((unsigned)seed); }

// ---------------------------------------------------------------- String

std::string String::fmtInt(long v, unsigned char base) {
    if (v < 0 && base == 10) return "-" + fmtUInt((unsigned long)(-v), base);
    return fmtUInt((unsigned long)v, base);
}

std::string String::fmtUInt(unsigned long v, unsigned char base) {
    if (base < 2 || base > 36) base = 10;
    std::string out;
    do {
        int d = (int)(v % base);
        out.insert(out.begin(), (char)(d < 10 ? '0' + d : 'A' + d - 10));
        v /= base;
    } while (v);
    return out;
}

std::string String::fmtDouble(double v, unsigned int decimals) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", (int)decimals, v);
    return buf;
}

void String::trim() {
    size_t b = 0, e = s_.size();
    while (b < e && isspace((unsigned char)s_[b])) b++;
    while (e > b && isspace((unsigned char)s_[e - 1])) e--;
    s_ = s_.substr(b, e - b);
}

void String::toLowerCase() {
    for (auto& c : s_) c = (char)tolower((unsigned char)c);
}

// ---------------------------------------------------------------- Print

size_t Print::print(const String& s) { return write((const uint8_t*)s.c_str(), s.length()); }

size_t Print::print(long v, int base) {
    std::string s = String(v, (unsigned char)base).str();
    return write(s.c_str());
}

size_t Print::print(unsigned long v, int base) {
    std::string s = String(v, (unsigned char)base).str();
    return write(s.c_str());
}

size_t Print::print(double v, int digits) {
    std::string s = String(v, (unsigned int)digits).str();
    return write(s.c_str());
}

size_t Print::printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (len < 0) {
        va_end(args);
        return 0;
    }
    std::vector<char> buf((size_t)len + 1);
    vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);
    return write((const uint8_t*)buf.data(), (size_t)len);
}

// ---------------------------------------------------------------- Serial (stdio)

HardwareSerial Serial;

static std::string stdinBuffer;
static bool stdinClosed = false;

// Pulls whatever is waiting on stdin without blocking the main loop.
static void pumpStdin() {
    if (stdinClosed) return;
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    while (poll(&pfd, 1, 0) > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
        char buf[256];
        ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            stdinClosed = true;
            return;
        }
        stdinBuffer.append(buf, (size_t)n);
    }
}

int HardwareSerial::available() {
    pumpStdin();
    return (int)stdinBuffer.size();
}

int HardwareSerial::read() {
    pumpStdin();
    if (stdinBuffer.empty()) return -1;
    int c = (unsigned char)stdinBuffer[0];
    stdinBuffer.erase(0, 1);
    return c;
}

int HardwareSerial::peek() {
    pumpStdin();
    return stdinBuffer.empty() ? -1 : (unsigned char)stdinBuffer[0];
}

size_t HardwareSerial::write(uint8_t c) { return write(&c, 1); }

size_t HardwareSerial::write(const uint8_t* buf, size_t size) {
    size_t n = fwrite(buf, 1, size, stdout);
    fflush(stdout);
    return n;
}

void HardwareSerial::flush() { fflush(stdout); }

// ---------------------------------------------------------------- ESP.restart()

EspClass ESP;

static char** savedArgv = nullptr;

void hostSetArgv(int, char** argv) { savedArgv = argv; }

void EspClass::restart() {
    fflush(stdout);
    if (savedArgv) {
        execv("/proc/self/exe", savedArgv);
        perror("restart: execv failed");
    }
    exit(0);
}
