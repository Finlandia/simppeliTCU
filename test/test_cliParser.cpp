// Basic unit tests for simppeliTCU/cliParser.cpp
// Serial (host shim) writes to stdout, which GoogleTest captures; input comes from stdin.
// 'reboot' and 'factory-reset' are not tested: ESP.restart() re-executes the process.
#include <gtest/gtest.h>

#include <string>
#include <unistd.h>

#include "cliParser.h"
#include "configuration.h"
#include "host_test_helpers.h"

void executeCommand(char* cmd);  // not in cliParser.h

class CliParser : public ::testing::Test {
protected:
    void SetUp() override { prefsPath = useFreshConfiguration(); }
    void TearDown() override { removeConfiguration(prefsPath); }

    static std::string run(const char* command) {
        std::string cmd = command;  // executeCommand tokenizes in place
        ::testing::internal::CaptureStdout();
        executeCommand(&cmd[0]);
        return ::testing::internal::GetCapturedStdout();
    }

    std::string prefsPath;
};

TEST_F(CliParser, GetPrintsValue) {
    EXPECT_EQ(run("get mqtt_port"), "8883\r\n");
    EXPECT_EQ(run("get hostName"), "leaf\r\n");
    EXPECT_EQ(run("get password"), "****\r\n");
}

TEST_F(CliParser, SetUpdatesConfiguration) {
    EXPECT_EQ(run("set mqtt_server broker.example"), "Value updated.\r\n");
    EXPECT_STREQ(getMqttServer(), "broker.example");

    EXPECT_EQ(run("set tracking true"), "Value updated.\r\n");
    EXPECT_TRUE(getTrackingEnabled());
}

TEST_F(CliParser, SetReportsValidationErrors) {
    EXPECT_NE(run("set mqtt_port 70000").find("Invalid port number"), std::string::npos);
    EXPECT_EQ(getMqttPort(), 8883);
    EXPECT_NE(run("set hostName -bad").find("hyphen"), std::string::npos);
    EXPECT_NE(run("set tracking maybe").find("must be 'true'"), std::string::npos);
}

TEST_F(CliParser, UnknownInput) {
    EXPECT_EQ(run("frobnicate"), "Unknown command.\r\n");
    EXPECT_EQ(run("get nothing"), "Unknown parameter.\r\n");
    EXPECT_EQ(run("set nothing 1"), "Unknown parameter.\r\n");
    EXPECT_EQ(run("get"), "Usage: get <key>\r\n");
}

TEST_F(CliParser, ListHidesSecretsByDefault) {
    setMqttPassword("hunter2");
    const std::string out = run("list");
    EXPECT_NE(out.find("mqtt_password: ****"), std::string::npos);
    EXPECT_EQ(out.find("hunter2"), std::string::npos);
    EXPECT_NE(run("list --secrets").find("mqtt_password: hunter2"), std::string::npos);
}

// The host Serial shim reads stdin; this replaces stdin with a pipe holding one command line.
// Only one test may do this: the shim stops reading once stdin reports end-of-file.
TEST_F(CliParser, ProcessSerialInputExecutesLine) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    const char input[] = "get hostName\n";
    ASSERT_EQ(write(fds[1], input, sizeof(input) - 1), (ssize_t)(sizeof(input) - 1));
    close(fds[1]);
    ASSERT_GE(dup2(fds[0], STDIN_FILENO), 0);
    close(fds[0]);

    ::testing::internal::CaptureStdout();
    processSerialInput();
    EXPECT_EQ(::testing::internal::GetCapturedStdout(), "leaf\r\n");
}
