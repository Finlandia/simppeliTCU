#pragma once

#include <cstdint>

class MDNSResponder {
public:
    bool begin(const char*) { return true; }
    void end() {}
    bool addService(const char*, const char*, uint16_t) { return true; }
};

inline MDNSResponder MDNS;
