#include "Arduino.h"

unsigned long fakeMillis = 0;
FakeSerial Serial;

unsigned long millis() {
    return fakeMillis;
}
