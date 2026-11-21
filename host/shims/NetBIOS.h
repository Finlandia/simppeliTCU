#pragma once

class NetBIOS {
public:
    bool begin(const char*) { return true; }
    void end() {}
};

inline NetBIOS NBNS;
