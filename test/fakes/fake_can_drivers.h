// Fakes for the hardware and application interfaces used by canInterface.cpp:
//   - ESP-IDF TWAI driver (types from host/shims/driver/twai.h)
//   - MCP2515 second CAN (secondCan.h)
//   - handleReceivedMessage() (normally implemented by canLeafZE1.cpp)
// Each fake records its calls and returns results the test configures.
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "driver/twai.h"
#include "secondCan.h"

struct FakeCanFrame {
    uint32_t id;
    uint8_t len;
    uint8_t data[8];
};

struct FakeTwai {
    // Configured results
    esp_err_t installResult = ESP_OK;
    esp_err_t startResult = ESP_OK;
    esp_err_t transmitResult = ESP_OK;

    // Recorded calls
    bool installCalled = false;
    twai_general_config_t generalConfig{};
    twai_timing_config_t timingConfig{};
    twai_filter_config_t filterConfig{};
    bool startCalled = false;
    std::vector<twai_message_t> transmitted;
    std::vector<TickType_t> transmitTicks;
    int receiveCalls = 0;
    std::vector<TickType_t> receiveTicks;

    // Frames returned by twai_receive(), oldest first
    std::deque<twai_message_t> rxQueue;
};

struct FakeSecondCan {
    // Configured results
    bool setupResult = true;
    bool sendResult = true;

    // Recorded calls
    bool setupCalled = false;
    SecondCanPins pins{};
    std::vector<FakeCanFrame> sent;

    // Frames returned by readSecondCANMessage(), oldest first
    std::deque<FakeCanFrame> rxQueue;
};

extern FakeTwai fakeTwai;
extern FakeSecondCan fakeSecondCan;
extern std::vector<FakeCanFrame> handledMessages;  // calls to handleReceivedMessage()

void resetCanDriverFakes();

FakeCanFrame makeFrame(uint32_t id, std::vector<uint8_t> bytes);
twai_message_t makeTwaiMessage(uint32_t id, std::vector<uint8_t> bytes);
