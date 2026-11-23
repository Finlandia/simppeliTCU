// Fake of the autowp-mcp2515 library API used by secondCan.cpp.
// Results are configured and calls recorded through fakeMcp2515 (fake_mcp2515.cpp).
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "SPI.h"

struct can_frame {
    uint32_t can_id;
    uint8_t can_dlc;
    uint8_t data[8];
};

enum CAN_SPEED { CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_1000KBPS };

class MCP2515 {
public:
    enum ERROR { ERROR_OK = 0, ERROR_FAIL = 1, ERROR_ALLTXBUSY = 2, ERROR_NOMSG = 5 };

    MCP2515(uint8_t cs, uint32_t spiClock, SPIClass* spi);
    ERROR reset();
    ERROR setBitrate(CAN_SPEED speed);
    ERROR setNormalMode();
    ERROR sendMessage(const struct can_frame* frame);
    ERROR readMessage(struct can_frame* frame);
};

struct FakeMcp2515 {
    // Configured results
    MCP2515::ERROR setBitrateResult = MCP2515::ERROR_OK;
    MCP2515::ERROR sendResult = MCP2515::ERROR_OK;

    // Recorded calls
    uint8_t cs = 0;
    uint32_t spiClock = 0;
    CAN_SPEED bitrate = CAN_125KBPS;
    bool normalMode = false;
    std::vector<can_frame> sent;

    // Frames returned by readMessage(), oldest first
    std::deque<can_frame> rxQueue;
};

extern FakeMcp2515 fakeMcp2515;
