// Compiles the Arduino sketch as regular C++.
// The Arduino IDE auto-generates prototypes for .ino files; a plain C++ compiler
// does not, so functions used before their definition are declared here.
#include <Arduino.h>

void logSequenceStart();

#include "simppeliTCU.ino"
