// Host-only configuration hooks, set from main.cpp command-line options.
#pragma once

void hostSetArgv(int argc, char** argv);

void hostWebServerConfigure(const char* bindAddress, int port);

void hostPreferencesSetPath(const char* path);
// Creates the preferences file with host-friendly defaults if it does not exist.
void hostSeedPreferences();

// bus 0 = CAN (TWAI, EV-CAN), bus 1 = second CAN (emulated MCP2515, AV-CAN)
bool hostCanUseSocketCan(int bus, const char* iface);
// Replay frames logged on this interface name go to the second CAN (default "vcan1")
void hostCanSetReplayCan2Iface(const char* iface);
bool hostCanUseReplay(const char* path, bool loop);
