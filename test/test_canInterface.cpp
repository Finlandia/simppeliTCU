// Unit tests for simppeliTCU/canInterface.cpp (TWAI driver glue, frame logging, second CAN).
// Built twice by CMake: with USE_SECOND_CAN=1 (LilyGO T-2Can default) and USE_SECOND_CAN=0.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "Arduino.h"
#include "canInterface.h"
#include "fake_can_drivers.h"
#include "featureFlags.h"
#include "hardware.h"

namespace {

class CanInterface : public ::testing::Test {
protected:
    void SetUp() override {
        resetCanDriverFakes();
        Serial.reset();
        fakeMillis = 0;
        resetCanLogTimestamps();
    }

    static bool outputContains(const std::string& text) {
        return Serial.output.find(text) != std::string::npos;
    }
};

// ---------------------------------------------------------------- sendCAN

TEST_F(CanInterface, SendCAN_TransmitsStandardDataFrame) {
    const uint8_t data[] = {0x4E, 0x08, 0x00, 0x00};
    sendCAN(0x56E, data, sizeof(data));

    ASSERT_EQ(fakeTwai.transmitted.size(), 1u);
    const twai_message_t& msg = fakeTwai.transmitted[0];
    EXPECT_EQ(msg.identifier, 0x56Eu);
    EXPECT_EQ(msg.extd, 0u);
    EXPECT_EQ(msg.rtr, 0u);
    EXPECT_EQ(msg.data_length_code, 4u);
    EXPECT_EQ(0, memcmp(msg.data, data, sizeof(data)));
    EXPECT_EQ(fakeTwai.transmitTicks[0], pdMS_TO_TICKS(10));
}

TEST_F(CanInterface, SendCAN_TemplateOverloadSendsWholeMessage) {
    const CANMessage<2> msg = {0x68C, {0xAA, 0x55}};
    sendCAN(msg);

    ASSERT_EQ(fakeTwai.transmitted.size(), 1u);
    EXPECT_EQ(fakeTwai.transmitted[0].identifier, 0x68Cu);
    EXPECT_EQ(fakeTwai.transmitted[0].data_length_code, 2u);
    EXPECT_EQ(fakeTwai.transmitted[0].data[0], 0xAA);
    EXPECT_EQ(fakeTwai.transmitted[0].data[1], 0x55);
}

TEST_F(CanInterface, SendCAN_LogsSuccessfulTransmit) {
    fakeMillis = 2345;
    const uint8_t data[] = {0x21, 0x98};
    sendCAN(0x55B, data, sizeof(data));

    EXPECT_EQ(Serial.output, "> 2.345 0x55B  0x21  0x98 \r\n");
}

TEST_F(CanInterface, SendCAN_LogsFailedTransmit) {
    fakeTwai.transmitResult = ESP_FAIL;
    const uint8_t data[] = {0x00};
    sendCAN(0x68C, data, sizeof(data));

    EXPECT_EQ(Serial.output, "! 0.000 0x68C  0x00 \r\n");
}

TEST_F(CanInterface, SendCAN_ZeroLengthFrame) {
    sendCAN(0x601, nullptr, 0);

    ASSERT_EQ(fakeTwai.transmitted.size(), 1u);
    EXPECT_EQ(fakeTwai.transmitted[0].data_length_code, 0u);
    EXPECT_EQ(Serial.output, "> 0.000 0x601 \r\n");
}

// ---------------------------------------------------------------- Frame logging

TEST_F(CanInterface, Log_TimestampIsRelativeToLastReset) {
    fakeMillis = 50000;
    resetCanLogTimestamps();
    fakeMillis = 51007;
    sendCAN(0x601, nullptr, 0);

    EXPECT_EQ(Serial.output, "> 1.007 0x601 \r\n");
}

TEST_F(CanInterface, Log_TimestampSurvivesMillisWraparound) {
    fakeMillis = static_cast<unsigned long>(-500);  // 500 ms before millis() wraps
    resetCanLogTimestamps();
    fakeMillis = 1500;
    sendCAN(0x601, nullptr, 0);

    EXPECT_EQ(Serial.output, "> 2.000 0x601 \r\n");
}

TEST_F(CanInterface, Log_PrintedWhenSerialHasExactlyEnoughRoom) {
    const std::string expected = "> 0.000 0x601 \r\n";
    Serial.writeCapacity = static_cast<int>(expected.size());
    sendCAN(0x601, nullptr, 0);

    EXPECT_EQ(Serial.output, expected);
}

TEST_F(CanInterface, Log_DroppedWhenSerialBufferFull) {
    const std::string line = "> 0.000 0x601 \r\n";
    Serial.writeCapacity = static_cast<int>(line.size()) - 1;
    sendCAN(0x601, nullptr, 0);

    EXPECT_TRUE(Serial.output.empty());
    // Logging must never block or skip the actual transmit
    EXPECT_EQ(fakeTwai.transmitted.size(), 1u);
}

TEST_F(CanInterface, Log_ClampsReceivedDlcAboveEight) {
    twai_message_t msg = makeTwaiMessage(0x123, {1, 2, 3, 4, 5, 6, 7, 8});
    msg.data_length_code = 15;  // non-compliant DLC: payload is still at most 8 bytes
    fakeTwai.rxQueue.push_back(msg);
    readAndHandleCANMessage();

    EXPECT_EQ(Serial.output,
              "< 0.000 0x123  0x01  0x02  0x03  0x04  0x05  0x06  0x07  0x08 \r\n");
}

// ---------------------------------------------------------------- readAndHandleCANMessage

TEST_F(CanInterface, Read_NoFramesDoesNothing) {
    readAndHandleCANMessage();

    EXPECT_TRUE(handledMessages.empty());
    EXPECT_TRUE(Serial.output.empty());
    ASSERT_GE(fakeTwai.receiveCalls, 1);
    EXPECT_EQ(fakeTwai.receiveTicks[0], 0u);  // never blocks the main loop
}

TEST_F(CanInterface, Read_DispatchesAndLogsFramesInOrder) {
    fakeMillis = 1234;
    fakeTwai.rxQueue.push_back(makeTwaiMessage(0x55B, {0x21, 0x98}));
    fakeTwai.rxQueue.push_back(makeTwaiMessage(0x54F, {0x78}));
    readAndHandleCANMessage();

    ASSERT_EQ(handledMessages.size(), 2u);
    EXPECT_EQ(handledMessages[0].id, 0x55Bu);
    EXPECT_EQ(handledMessages[0].len, 2u);
    EXPECT_EQ(handledMessages[0].data[0], 0x21);
    EXPECT_EQ(handledMessages[0].data[1], 0x98);
    EXPECT_EQ(handledMessages[1].id, 0x54Fu);
    EXPECT_EQ(handledMessages[1].len, 1u);
    EXPECT_EQ(handledMessages[1].data[0], 0x78);

    EXPECT_EQ(Serial.output,
              "< 1.234 0x55B  0x21  0x98 \r\n"
              "< 1.234 0x54F  0x78 \r\n");
}

TEST_F(CanInterface, Read_HandlesAtMostFourFramesPerCall) {
    for (uint32_t id = 0x100; id < 0x106; id++) {
        fakeTwai.rxQueue.push_back(makeTwaiMessage(id, {0x00}));
    }

    readAndHandleCANMessage();
    ASSERT_EQ(handledMessages.size(), 4u);
    EXPECT_EQ(handledMessages[3].id, 0x103u);
    EXPECT_EQ(fakeTwai.rxQueue.size(), 2u);

    readAndHandleCANMessage();
    ASSERT_EQ(handledMessages.size(), 6u);
    EXPECT_EQ(handledMessages[5].id, 0x105u);
    EXPECT_TRUE(fakeTwai.rxQueue.empty());
}

// ---------------------------------------------------------------- setupCAN

TEST_F(CanInterface, Setup_ConfiguresAndStartsTwaiDriver) {
    setupCAN();

    ASSERT_TRUE(fakeTwai.installCalled);
    EXPECT_EQ(fakeTwai.timingConfig.bitrate, 500000u);
    EXPECT_EQ(fakeTwai.generalConfig.tx_io, CAN_TX_PIN);
    EXPECT_EQ(fakeTwai.generalConfig.rx_io, CAN_RX_PIN);
    EXPECT_EQ(fakeTwai.generalConfig.mode, TWAI_MODE_NORMAL);
    EXPECT_EQ(fakeTwai.generalConfig.rx_queue_len, 32u);
    EXPECT_EQ(fakeTwai.filterConfig.acceptance_mask, 0xFFFFFFFFu);  // accept all
    EXPECT_TRUE(fakeTwai.startCalled);

    EXPECT_TRUE(outputContains("CAN driver installed\r\n"));
    EXPECT_TRUE(outputContains("CAN driver started\r\n"));
}

TEST_F(CanInterface, Setup_ReportsTwaiDriverFailures) {
    fakeTwai.installResult = ESP_FAIL;
    fakeTwai.startResult = ESP_FAIL;
    setupCAN();

    EXPECT_TRUE(outputContains("Failed to install CAN driver\r\n"));
    EXPECT_TRUE(outputContains("Failed to start CAN driver\r\n"));
    EXPECT_FALSE(outputContains("CAN driver installed"));
    EXPECT_FALSE(outputContains("CAN driver started"));
}

// ---------------------------------------------------------------- getCanBusState

TEST_F(CanInterface, BusState_MapsTwaiStates) {
    fakeTwai.state = TWAI_STATE_RUNNING;
    EXPECT_STREQ(getCanBusState(), "running");
    fakeTwai.state = TWAI_STATE_STOPPED;
    EXPECT_STREQ(getCanBusState(), "stopped");
    fakeTwai.state = TWAI_STATE_BUS_OFF;
    EXPECT_STREQ(getCanBusState(), "bus_off");
    fakeTwai.state = TWAI_STATE_RECOVERING;
    EXPECT_STREQ(getCanBusState(), "recovering");
}

TEST_F(CanInterface, BusState_ReportsDriverNotInstalled) {
    fakeTwai.statusResult = ESP_ERR_INVALID_STATE;
    EXPECT_STREQ(getCanBusState(), "not_installed");
}

TEST_F(CanInterface, ErrorCounters_ReadFromTwaiStatus) {
    fakeTwai.txErrorCounter = 128;
    fakeTwai.rxErrorCounter = 7;
    fakeTwai.rxMissedCount = 42;

    CanBusErrorCounters counters{};
    ASSERT_TRUE(getCanBusErrorCounters(counters));
    EXPECT_EQ(counters.txErrors, 128u);
    EXPECT_EQ(counters.rxErrors, 7u);
    EXPECT_EQ(counters.rxMissed, 42u);
}

TEST_F(CanInterface, ErrorCounters_UnavailableWhenDriverNotInstalled) {
    fakeTwai.statusResult = ESP_ERR_INVALID_STATE;
    CanBusErrorCounters counters{};
    EXPECT_FALSE(getCanBusErrorCounters(counters));
}

#if USE_SECOND_CAN

// ---------------------------------------------------------------- Second CAN enabled

TEST_F(CanInterface, SecondCan_SetupUsesBoardPins) {
    setupCAN();

    ASSERT_TRUE(fakeSecondCan.setupCalled);
    EXPECT_EQ(fakeSecondCan.pins.cs, MCP2515_CS);
    EXPECT_EQ(fakeSecondCan.pins.sclk, MCP2515_SCLK);
    EXPECT_EQ(fakeSecondCan.pins.mosi, MCP2515_MOSI);
    EXPECT_EQ(fakeSecondCan.pins.miso, MCP2515_MISO);
    EXPECT_EQ(fakeSecondCan.pins.rst, MCP2515_RST);
    EXPECT_EQ(fakeSecondCan.pins.intPin, MCP2515_INT);
    EXPECT_TRUE(outputContains("Second CAN initialized\r\n"));
}

TEST_F(CanInterface, SecondCan_SetupReportsFailure) {
    fakeSecondCan.setupResult = false;
    setupCAN();

    EXPECT_TRUE(outputContains("Failed to initialize second CAN\r\n"));
}

TEST_F(CanInterface, SecondCan_SendMirrorsFrame) {
    const uint8_t data[] = {0x11, 0x00, 0x00, 0x00};
    sendCAN(0x56E, data, sizeof(data));

    ASSERT_EQ(fakeSecondCan.sent.size(), 1u);
    EXPECT_EQ(fakeSecondCan.sent[0].id, 0x56Eu);
    EXPECT_EQ(fakeSecondCan.sent[0].len, 4u);
    EXPECT_EQ(0, memcmp(fakeSecondCan.sent[0].data, data, sizeof(data)));
    EXPECT_FALSE(outputContains("second CAN"));
}

TEST_F(CanInterface, SecondCan_SendReportsFailure) {
    fakeSecondCan.sendResult = false;
    sendCAN(0x601, nullptr, 0);

    EXPECT_EQ(Serial.output,
              "> 0.000 0x601 \r\n"
              "! Failed to send message on second CAN\r\n");
}

TEST_F(CanInterface, SecondCan_SendFailureMessageDroppedWhenSerialFull) {
    fakeSecondCan.sendResult = false;
    Serial.writeCapacity = 10;  // too small for both the frame log and the error
    sendCAN(0x601, nullptr, 0);

    EXPECT_TRUE(Serial.output.empty());
}

TEST_F(CanInterface, SecondCan_ReadDispatchesAndLogsFrames) {
    fakeMillis = 42;
    fakeSecondCan.rxQueue.push_back(makeFrame(0x5C5, {0x40, 0x01}));
    readAndHandleCANMessage();

    ASSERT_EQ(handledMessages.size(), 1u);
    EXPECT_EQ(handledMessages[0].id, 0x5C5u);
    EXPECT_EQ(handledMessages[0].len, 2u);
    EXPECT_EQ(handledMessages[0].data[0], 0x40);
    EXPECT_EQ(handledMessages[0].data[1], 0x01);
    EXPECT_EQ(Serial.output, "2< 0.042 0x5C5  0x40  0x01 \r\n");
}

TEST_F(CanInterface, SecondCan_ReadHandlesAtMostFourFramesPerCall) {
    for (uint32_t id = 0x200; id < 0x206; id++) {
        fakeSecondCan.rxQueue.push_back(makeFrame(id, {0x00}));
    }

    readAndHandleCANMessage();
    EXPECT_EQ(handledMessages.size(), 4u);
    EXPECT_EQ(fakeSecondCan.rxQueue.size(), 2u);
}

TEST_F(CanInterface, SecondCan_PrimaryBusIsHandledFirst) {
    fakeTwai.rxQueue.push_back(makeTwaiMessage(0x100, {0x01}));
    fakeSecondCan.rxQueue.push_back(makeFrame(0x200, {0x02}));
    readAndHandleCANMessage();

    ASSERT_EQ(handledMessages.size(), 2u);
    EXPECT_EQ(handledMessages[0].id, 0x100u);
    EXPECT_EQ(handledMessages[1].id, 0x200u);
}

#else

// ---------------------------------------------------------------- Second CAN disabled

TEST_F(CanInterface, NoSecondCan_SetupReportsDisabled) {
    setupCAN();

    EXPECT_FALSE(fakeSecondCan.setupCalled);
    EXPECT_TRUE(outputContains("Second CAN disabled by configuration\r\n"));
}

TEST_F(CanInterface, NoSecondCan_SendUsesOnlyTwai) {
    const uint8_t data[] = {0x00};
    sendCAN(0x68C, data, sizeof(data));

    EXPECT_EQ(fakeTwai.transmitted.size(), 1u);
    EXPECT_TRUE(fakeSecondCan.sent.empty());
}

TEST_F(CanInterface, NoSecondCan_ReadIgnoresSecondBus) {
    fakeSecondCan.rxQueue.push_back(makeFrame(0x200, {0x02}));
    readAndHandleCANMessage();

    EXPECT_TRUE(handledMessages.empty());
    EXPECT_EQ(fakeSecondCan.rxQueue.size(), 1u);
}

#endif

}  // namespace
