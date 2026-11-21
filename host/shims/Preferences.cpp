#include "Preferences.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include "host_config.h"

static std::string prefsPath = "simppeliTCU_host_prefs.txt";

void hostPreferencesSetPath(const char* path) { prefsPath = path; }

void hostSeedPreferences() {
    std::ifstream existing(prefsPath);
    if (existing.good()) return;

    // A non-empty SSID makes the firmware start its web server; plain MQTT because TLS
    // is not implemented in the host build.
    std::ofstream out(prefsPath);
    out << "ssid=simppeliTCU-host\n"
        << "mqtt_tls=0\n"
        << "mqtt_port=1883\n";
    fprintf(stderr, "[host] Created %s with host defaults (ssid set, mqtt_tls false, mqtt_port 1883)\n",
            prefsPath.c_str());
}

bool Preferences::begin(const char*, bool readOnly, const char*) {
    readOnly_ = readOnly;
    values_.clear();
    std::ifstream in(prefsPath);
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        values_[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return true;
}

bool Preferences::save() {
    if (readOnly_) return false;
    std::ofstream out(prefsPath, std::ios::trunc);
    for (auto& kv : values_) out << kv.first << '=' << kv.second << '\n';
    return out.good();
}

bool Preferences::clear() {
    values_.clear();
    return save();
}

bool Preferences::isKey(const char* key) { return values_.count(key) != 0; }

size_t Preferences::putString(const char* key, const char* value) {
    // Values are stored one per line, so line breaks cannot be persisted
    std::string v = value ? value : "";
    for (auto& c : v)
        if (c == '\n' || c == '\r') c = ' ';
    values_[key] = v;
    return save() ? v.size() : 0;
}

size_t Preferences::getString(const char* key, char* value, size_t maxLen) {
    auto it = values_.find(key);
    if (it == values_.end() || maxLen == 0) return 0;
    size_t len = it->second.size();
    if (len + 1 > maxLen) return 0;  // same as ESP32: buffer too small
    memcpy(value, it->second.c_str(), len + 1);
    return len + 1;
}

String Preferences::getString(const char* key, const String& defaultValue) {
    auto it = values_.find(key);
    return it == values_.end() ? defaultValue : String(it->second);
}

size_t Preferences::putInt(const char* key, int32_t value) {
    values_[key] = std::to_string(value);
    return save() ? sizeof(value) : 0;
}

int32_t Preferences::getInt(const char* key, int32_t defaultValue) {
    auto it = values_.find(key);
    return it == values_.end() ? defaultValue : (int32_t)strtol(it->second.c_str(), nullptr, 10);
}

size_t Preferences::putBool(const char* key, bool value) {
    values_[key] = value ? "1" : "0";
    return save() ? 1 : 0;
}

bool Preferences::getBool(const char* key, bool defaultValue) {
    auto it = values_.find(key);
    if (it == values_.end()) return defaultValue;
    return it->second == "1" || it->second == "true";
}
