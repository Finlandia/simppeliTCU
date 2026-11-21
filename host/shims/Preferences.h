// Preferences (NVS) replacement persisted to a key=value text file.
#pragma once

#include <map>
#include <string>
#include "WString.h"

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false, const char* partition = nullptr);
    void end() {}
    bool clear();
    bool isKey(const char* key);

    size_t putString(const char* key, const char* value);
    size_t putString(const char* key, const String& value) { return putString(key, value.c_str()); }
    size_t getString(const char* key, char* value, size_t maxLen);
    String getString(const char* key, const String& defaultValue = String());

    size_t putInt(const char* key, int32_t value);
    int32_t getInt(const char* key, int32_t defaultValue = 0);

    size_t putBool(const char* key, bool value);
    bool getBool(const char* key, bool defaultValue = false);

private:
    bool save();
    bool readOnly_ = false;
    std::map<std::string, std::string> values_;
};
