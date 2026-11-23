// Helpers for tests built against the host shims (host/shims).
#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <unistd.h>

#include "configuration.h"
#include "host_config.h"

// Points the Preferences shim at an empty temporary file and loads the default configuration.
// Call from SetUp(), so every test starts with default settings and no file is left behind.
inline std::string useFreshConfiguration() {
    static const std::string path =
        ::testing::TempDir() + "simppeliTCU_test_prefs_" + std::to_string(getpid()) + ".txt";
    std::remove(path.c_str());
    hostPreferencesSetPath(path.c_str());
    initConfiguration();
    return path;
}

inline void removeConfiguration(const std::string& path) {
    std::remove(path.c_str());
}
