#include "fake_can_drivers.h"

#include <cstring>

FakeTwai fakeTwai;
FakeSecondCan fakeSecondCan;
std::vector<FakeCanFrame> handledMessages;

void resetCanDriverFakes() {
    fakeTwai = FakeTwai{};
    fakeSecondCan = FakeSecondCan{};
    handledMessages.clear();
}

FakeCanFrame makeFrame(uint32_t id, std::vector<uint8_t> bytes) {
    FakeCanFrame f{};
    f.id = id;
    f.len = static_cast<uint8_t>(bytes.size() > 8 ? 8 : bytes.size());
    memcpy(f.data, bytes.data(), f.len);
    return f;
}

twai_message_t makeTwaiMessage(uint32_t id, std::vector<uint8_t> bytes) {
    twai_message_t m{};
    m.identifier = id;
    m.data_length_code = static_cast<uint8_t>(bytes.size() > 8 ? 8 : bytes.size());
    memcpy(m.data, bytes.data(), m.data_length_code);
    return m;
}

// ---------------------------------------------------------------- TWAI driver

esp_err_t twai_driver_install(const twai_general_config_t* g, const twai_timing_config_t* t,
                              const twai_filter_config_t* f) {
    fakeTwai.installCalled = true;
    fakeTwai.generalConfig = *g;
    fakeTwai.timingConfig = *t;
    fakeTwai.filterConfig = *f;
    return fakeTwai.installResult;
}

esp_err_t twai_start() {
    fakeTwai.startCalled = true;
    return fakeTwai.startResult;
}

esp_err_t twai_transmit(const twai_message_t* message, TickType_t ticks) {
    fakeTwai.transmitted.push_back(*message);
    fakeTwai.transmitTicks.push_back(ticks);
    return fakeTwai.transmitResult;
}

esp_err_t twai_receive(twai_message_t* message, TickType_t ticks) {
    fakeTwai.receiveCalls++;
    fakeTwai.receiveTicks.push_back(ticks);
    if (fakeTwai.rxQueue.empty()) return ESP_ERR_TIMEOUT;
    *message = fakeTwai.rxQueue.front();
    fakeTwai.rxQueue.pop_front();
    return ESP_OK;
}

// ---------------------------------------------------------------- Second CAN (MCP2515)

bool setupSecondCAN(const SecondCanPins& pins) {
    fakeSecondCan.setupCalled = true;
    fakeSecondCan.pins = pins;
    return fakeSecondCan.setupResult;
}

bool sendSecondCAN(uint32_t id, const uint8_t* data, uint8_t len) {
    FakeCanFrame f{};
    f.id = id;
    f.len = len;
    memcpy(f.data, data, len > 8 ? 8 : len);
    fakeSecondCan.sent.push_back(f);
    return fakeSecondCan.sendResult;
}

bool readSecondCANMessage(uint32_t* id, uint8_t* data, uint8_t* len) {
    if (fakeSecondCan.rxQueue.empty()) return false;
    const FakeCanFrame f = fakeSecondCan.rxQueue.front();
    fakeSecondCan.rxQueue.pop_front();
    *id = f.id;
    *len = f.len;
    memcpy(data, f.data, f.len);
    return true;
}

// ---------------------------------------------------------------- Application handler

void handleReceivedMessage(uint32_t identifier, uint8_t* data, uint8_t len) {
    FakeCanFrame f{};
    f.id = identifier;
    f.len = len;
    memcpy(f.data, data, len > 8 ? 8 : len);
    handledMessages.push_back(f);
}
