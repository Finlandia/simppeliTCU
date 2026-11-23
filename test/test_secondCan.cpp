// Basic unit tests for simppeliTCU/secondCan.cpp (built with USE_SECOND_CAN=1)
// The MCP2515 driver and SPI are faked in fakes/second_can/.
#include <gtest/gtest.h>

#include "hardware.h"
#include "mcp2515.h"
#include "secondCan.h"

class SecondCan : public ::testing::Test {
protected:
    // secondCan.cpp keeps its driver instance after setup, so every test sets it up first
    void SetUp() override {
        fakeMcp2515 = FakeMcp2515{};
        ASSERT_TRUE(setupSecondCAN(pins));
    }

    const SecondCanPins pins = {MCP2515_CS, MCP2515_SCLK, MCP2515_MOSI, MCP2515_MISO, MCP2515_RST, MCP2515_INT};
};

TEST_F(SecondCan, SetupConfiguresDriver) {
    EXPECT_TRUE(SPI.began);
    EXPECT_EQ(SPI.sck, MCP2515_SCLK);
    EXPECT_EQ(SPI.ss, MCP2515_CS);
    EXPECT_EQ(fakeMcp2515.cs, MCP2515_CS);
    EXPECT_EQ(fakeMcp2515.spiClock, (uint32_t)MCP2515_SPI_CLOCK);
    EXPECT_EQ(fakeMcp2515.bitrate, CAN_500KBPS);
    EXPECT_TRUE(fakeMcp2515.normalMode);
}

TEST_F(SecondCan, SetupFailsWhenBitrateRejected) {
    fakeMcp2515 = FakeMcp2515{};
    fakeMcp2515.setBitrateResult = MCP2515::ERROR_FAIL;
    EXPECT_FALSE(setupSecondCAN(pins));
    EXPECT_FALSE(fakeMcp2515.normalMode);
}

TEST_F(SecondCan, SendClampsLengthToEightBytes) {
    const uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT_TRUE(sendSecondCAN(0x56E, data, sizeof(data)));
    ASSERT_EQ(fakeMcp2515.sent.size(), 1u);
    EXPECT_EQ(fakeMcp2515.sent[0].can_id, 0x56Eu);
    EXPECT_EQ(fakeMcp2515.sent[0].can_dlc, 8u);

    fakeMcp2515.sendResult = MCP2515::ERROR_ALLTXBUSY;
    EXPECT_FALSE(sendSecondCAN(0x56E, data, 1));
}

TEST_F(SecondCan, ReadReturnsQueuedFrame) {
    uint32_t id = 0;
    uint8_t data[8] = {};
    uint8_t len = 0;
    EXPECT_FALSE(readSecondCANMessage(&id, data, &len));

    fakeMcp2515.rxQueue.push_back(can_frame{0x5C5, 2, {0x40, 0x01}});
    ASSERT_TRUE(readSecondCANMessage(&id, data, &len));
    EXPECT_EQ(id, 0x5C5u);
    EXPECT_EQ(len, 2u);
    EXPECT_EQ(data[0], 0x40);
}
