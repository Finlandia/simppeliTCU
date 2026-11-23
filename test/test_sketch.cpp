// Basic unit tests for simppeliTCU/simppeliTCU.ino (compiled through host/sketch.cpp)
// Only the CAN callbacks are exercised; setup()/loop() would start WiFi, the web server and MQTT.
#include <gtest/gtest.h>

#include <string>

#include "canLeafZE1.h"
#include "configuration.h"
#include "host_test_helpers.h"
#include "vehicleTypes.h"

// Globals defined in simppeliTCU.ino
extern float currentSOC;
extern float cabinTemp;
extern bool isChargingNow;
extern ChargerState currentChargerState;
extern bool isHvacOn;
extern int8_t lockState;
extern float currentFanSpeed;
extern LeafGpsData currentGpsData;
void resetData();
void handleRawSOC(float soc);
void handleCabinTemp(float temp);
void handleChargerStatus(bool isCharging, ChargerState state);
void handleHVACStatus(bool isOn);
void handleFanSpeed(float speed);
void handleLockStatus(bool locked);
void handleGPSData(const LeafGpsData& gps);

class Sketch : public ::testing::Test {
protected:
    void SetUp() override {
        prefsPath = useFreshConfiguration();
        resetData();
        currentGpsData = {};
    }
    void TearDown() override { removeConfiguration(prefsPath); }
    std::string prefsPath;
};

TEST_F(Sketch, CallbacksStoreDecodedValues) {
    handleRawSOC(80.5f);
    handleCabinTemp(19.0f);
    handleChargerStatus(true, ChargerState::CHARGING);
    handleLockStatus(true);

    EXPECT_FLOAT_EQ(currentSOC, 80.5f);
    EXPECT_FLOAT_EQ(cabinTemp, 19.0f);
    EXPECT_TRUE(isChargingNow);
    EXPECT_EQ(currentChargerState, ChargerState::CHARGING);
    EXPECT_EQ(lockState, 1);
}

TEST_F(Sketch, FanSpeedZeroIgnoredWhileHvacRuns) {
    handleHVACStatus(true);
    handleFanSpeed(57.0f);
    handleFanSpeed(0.0f);  // the car reports 0 during remote climate
    EXPECT_FLOAT_EQ(currentFanSpeed, 57.0f);
}

TEST_F(Sketch, GpsIgnoredUnlessTrackingEnabled) {
    LeafGpsData gps{};
    gps.latitude = 60.17;
    gps.longitude = 24.95;

    handleGPSData(gps);
    EXPECT_DOUBLE_EQ(currentGpsData.latitude, 0.0);

    setTrackingEnabled(true);
    handleGPSData(gps);
    EXPECT_DOUBLE_EQ(currentGpsData.latitude, 60.17);
}
