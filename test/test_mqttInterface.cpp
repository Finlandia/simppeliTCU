// Basic unit tests for simppeliTCU/mqttInterface.cpp
// No broker is used: the client is never connected, so only command parsing and the
// "not connected" paths run. setupMQTT() only configures the client and does not connect.
#include <gtest/gtest.h>

#include <string>

#include "configuration.h"
#include "host_test_helpers.h"
#include "mqttInterface.h"

void mqttCallback(char* topic, byte* payload, unsigned int length);  // not in mqttInterface.h

// Command callbacks normally implemented in simppeliTCU.ino
static int hvacOnCalls, hvacOffCalls, chargeOnCalls, refreshCalls, lockCalls, unlockCalls;
static float lastHvacSetpoint;
void handleMqttHvacOn(float setpoint) { hvacOnCalls++; lastHvacSetpoint = setpoint; }
void handleMqttHvacOff() { hvacOffCalls++; }
void handleMqttChargeOn() { chargeOnCalls++; }
void handleMqttRefresh() { refreshCalls++; }
void handleMqttLock() { lockCalls++; }
void handleMqttUnlock() { unlockCalls++; }

class MqttInterface : public ::testing::Test {
protected:
    void SetUp() override {
        prefsPath = useFreshConfiguration();
        setMqttUser("user");
        setVehicleId("leaf1");
        setupMQTT();
        hvacOnCalls = hvacOffCalls = chargeOnCalls = refreshCalls = lockCalls = unlockCalls = 0;
        lastHvacSetpoint = -1.0f;
    }
    void TearDown() override { removeConfiguration(prefsPath); }

    // Delivers a command as the OVMS app would: ovms/<user>/<vehicle>/client/<app>/command/<id>
    static void sendCommand(const char* payload) {
        std::string topic = "ovms/user/leaf1/client/app/command/1";
        ::testing::internal::CaptureStdout();  // keep the callback's logging out of the test output
        mqttCallback(&topic[0], (byte*)payload, (unsigned int)strlen(payload));
        ::testing::internal::GetCapturedStdout();
    }

    std::string prefsPath;
};

TEST_F(MqttInterface, DispatchesOvmsCommands) {
    sendCommand("climatecontrol on 21.5");
    EXPECT_EQ(hvacOnCalls, 1);
    EXPECT_FLOAT_EQ(lastHvacSetpoint, 21.5f);

    sendCommand("climatecontrol off");
    sendCommand("charge start");
    sendCommand("server v3 update modified");
    EXPECT_EQ(hvacOffCalls, 1);
    EXPECT_EQ(chargeOnCalls, 1);
    EXPECT_EQ(refreshCalls, 1);
}

TEST_F(MqttInterface, LockCommandsIgnorePin) {
    sendCommand("lock 1234");
    sendCommand("UNLOCK 1234\n");  // payload is trimmed and lower-cased
    EXPECT_EQ(lockCalls, 1);
    EXPECT_EQ(unlockCalls, 1);
}

TEST_F(MqttInterface, IgnoresUnknownCommandAndForeignTopic) {
    sendCommand("self destruct");

    std::string otherTopic = "ovms/someone/else/client/app/command/1";
    const char payload[] = "charge start";
    mqttCallback(&otherTopic[0], (byte*)payload, sizeof(payload) - 1);

    EXPECT_EQ(hvacOnCalls + hvacOffCalls + chargeOnCalls + refreshCalls + lockCalls + unlockCalls, 0);
}

TEST_F(MqttInterface, UpdatesWhileDisconnectedDoNotCrash) {
    mqttUpdateSOC(55.5f);
    mqttUpdateCabinTemp(21.0f);
    mqttUpdateCharging(true, ChargerState::CHARGING);
    mqttUpdateHVAC(true);
    mqttUpdateHVACSetpoint(22.0f);
    mqttUpdateFanSpeed(50.0f);
    mqttUpdateHeatingMode(true, false);
    mqttUpdateVentilationMode(VentilationMode::FACE);
    mqttUpdateDoors(false, false, false, false, true);
    mqttUpdateLock(true);
    mqttUpdateGPS(60.17, 24.95);
    mqttPublishStatus("hello");
    SUCCEED();
}
