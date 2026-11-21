// Host-only configuration hooks, set from main.cpp command-line options.
#pragma once

void hostSetArgv(int argc, char** argv);

void hostWebServerConfigure(const char* bindAddress, int port);

void hostPreferencesSetPath(const char* path);
// Creates the preferences file with host-friendly defaults if it does not exist.
void hostSeedPreferences();

bool hostCanUseSocketCan(const char* iface);
bool hostCanUseReplay(const char* path, bool loop);
