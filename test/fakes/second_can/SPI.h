// Fake of the ESP32 Arduino SPI class used by secondCan.cpp.
#pragma once

#include <cstdint>

class SPIClass {
public:
    void begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
        this->sck = sck;
        this->miso = miso;
        this->mosi = mosi;
        this->ss = ss;
        began = true;
    }

    bool began = false;
    int8_t sck = -1, miso = -1, mosi = -1, ss = -1;
};

extern SPIClass SPI;
