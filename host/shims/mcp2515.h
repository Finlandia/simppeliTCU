// autowp-mcp2515 library replacement: the second CAN interface (AV-CAN) is emulated by the
// host CAN backend (can_host.cpp), so it can be a SocketCAN interface or part of a log replay.
// Only the parts of the library API that secondCan.cpp uses are provided.
#pragma once

#include <linux/can.h>  // struct can_frame, CAN_EFF_FLAG: same layout as the library's can.h
#include <linux/can/raw.h>

#include <cstdint>

#include "SPI.h"

enum CAN_SPEED { CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_1000KBPS };

class MCP2515 {
public:
    enum ERROR { ERROR_OK = 0, ERROR_FAIL = 1, ERROR_ALLTXBUSY = 2, ERROR_FAILINIT = 3, ERROR_FAILTX = 4, ERROR_NOMSG = 5 };

    MCP2515(uint8_t, uint32_t = 10000000, SPIClass* = nullptr) {}

    ERROR reset();
    ERROR setBitrate(CAN_SPEED speed);
    ERROR setNormalMode();
    ERROR sendMessage(const struct can_frame* frame);
    ERROR readMessage(struct can_frame* frame);
};
