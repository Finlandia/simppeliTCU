// SPI stub: the MCP2515 is emulated (see mcp2515.h), so no SPI bus is needed.
#pragma once

#include <cstdint>

class SPIClass {
public:
    void begin(int8_t = -1, int8_t = -1, int8_t = -1, int8_t = -1) {}
};

extern SPIClass SPI;
