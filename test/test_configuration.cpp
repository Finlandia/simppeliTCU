// Basic unit tests for simppeliTCU/configuration.cpp (Preferences backed by a temp file)
#include <gtest/gtest.h>

#include <string>

#include "configuration.h"
#include "host_test_helpers.h"

class Configuration : public ::testing::Test {
protected:
    void SetUp() override { prefsPath = useFreshConfiguration(); }
    void TearDown() override { removeConfiguration(prefsPath); }
    std::string prefsPath;
};

TEST_F(Configuration, Defaults) {
    EXPECT_STREQ(getWifiSSID(), "");
    EXPECT_STREQ(getHostName(), "leaf");
    EXPECT_EQ(getMqttPort(), 8883);
    EXPECT_TRUE(getMqttTls());
    EXPECT_TRUE(getHassDiscoveryEnabled());
    EXPECT_TRUE(getNagDismissEnabled());
    EXPECT_FALSE(getLockingEnabled());
    EXPECT_FALSE(getTrackingEnabled());
}

TEST_F(Configuration, ValuesPersistAcrossReload) {
    EXPECT_EQ(setMqttServer("broker.example"), ConfigStatus::OK);
    EXPECT_EQ(setMqttPort(1883), ConfigStatus::OK);
    EXPECT_EQ(setLockingEnabled(true), ConfigStatus::OK);

    initConfiguration();

    EXPECT_STREQ(getMqttServer(), "broker.example");
    EXPECT_EQ(getMqttPort(), 1883);
    EXPECT_TRUE(getLockingEnabled());
}

TEST_F(Configuration, RejectsTooLongString) {
    const std::string tooLong(64, 'x');
    EXPECT_EQ(setWifiSSID(tooLong.c_str()), ConfigStatus::ERROR_LENGTH);
    EXPECT_STREQ(getWifiSSID(), "");
}

TEST_F(Configuration, ValidatesHostName) {
    EXPECT_EQ(setHostName("my-leaf"), ConfigStatus::OK);
    EXPECT_EQ(setHostName(""), ConfigStatus::ERROR_HOSTNAME_LENGTH);
    EXPECT_EQ(setHostName("-leaf"), ConfigStatus::ERROR_HOSTNAME_HYPHEN);
    EXPECT_EQ(setHostName("leaf_1"), ConfigStatus::ERROR_HOSTNAME_CHARS);
    EXPECT_STREQ(getHostName(), "my-leaf");
}

TEST_F(Configuration, ValidatesApPassword) {
    EXPECT_EQ(setApPassword(""), ConfigStatus::OK);
    EXPECT_EQ(setApPassword("short"), ConfigStatus::ERROR_PASSWORD_LENGTH);
    EXPECT_EQ(setApPassword("longenough"), ConfigStatus::OK);
}

TEST_F(Configuration, FactoryResetRestoresDefaults) {
    setVehicleId("myleaf");
    factoryReset();
    initConfiguration();
    EXPECT_STREQ(getVehicleId(), "");
}
