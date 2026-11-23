// Basic unit tests for simppeliTCU/hassDiscovery.cpp
// Uses the real PubSubClient with no broker, so publishing is not exercised yet.
#include <gtest/gtest.h>

#include <PubSubClient.h>
#include <cstring>
#include <string>

#include "hassDiscovery.h"
#include "host_test_helpers.h"
#include "stringBuffer.h"

class HassDiscovery : public ::testing::Test {
protected:
    void SetUp() override { prefsPath = useFreshConfiguration(); }
    void TearDown() override { removeConfiguration(prefsPath); }
    std::string prefsPath;
};

TEST_F(HassDiscovery, MetricTopicsAreOvmsCompatible) {
    EXPECT_STREQ(ovmsMetrics.soc, "metric/v/b/soc");
    EXPECT_STREQ(ovmsMetrics.connected, "metric/s/v3/connected");
    EXPECT_EQ(strncmp(ovmsMetrics.latitude, "metric/", 7), 0);
}

TEST_F(HassDiscovery, SkipsPublishingWhenDisconnected) {
    PubSubClient client;
    StringBuffer<128> prefix;
    prefix.copyFrom("ovms/user/leaf1/");

    mqttPublishHassDiscovery(client, prefix);
    EXPECT_FALSE(client.connected());
}
