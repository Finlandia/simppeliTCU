#include "mcp2515.h"

SPIClass SPI;
FakeMcp2515 fakeMcp2515;

MCP2515::MCP2515(uint8_t cs, uint32_t spiClock, SPIClass*) {
    fakeMcp2515.cs = cs;
    fakeMcp2515.spiClock = spiClock;
}

MCP2515::ERROR MCP2515::reset() {
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::setBitrate(CAN_SPEED speed) {
    fakeMcp2515.bitrate = speed;
    return fakeMcp2515.setBitrateResult;
}

MCP2515::ERROR MCP2515::setNormalMode() {
    fakeMcp2515.normalMode = true;
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::sendMessage(const struct can_frame* frame) {
    fakeMcp2515.sent.push_back(*frame);
    return fakeMcp2515.sendResult;
}

MCP2515::ERROR MCP2515::readMessage(struct can_frame* frame) {
    if (fakeMcp2515.rxQueue.empty()) return ERROR_NOMSG;
    *frame = fakeMcp2515.rxQueue.front();
    fakeMcp2515.rxQueue.pop_front();
    return ERROR_OK;
}
