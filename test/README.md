# Unit tests

Host-side GoogleTest suite for the firmware's CAN logic. Build and run instructions are in
[docs/compiling.md](../docs/compiling.md#running-unit-tests). On Linux:

```bash
cmake -S test -B test/build && cmake --build test/build && ctest --test-dir test/build
```

## Coverage report

Requires `gcovr` (`apt install gcovr` or `pip install gcovr`):

```bash
cmake -S test -B test/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage -O0"
cmake --build test/build
ctest --test-dir test/build
cmake --build test/build --target coverage
```

Open `test/build/coverage/index.html`. It lists only firmware sources (`simppeliTCU/`), and each
file links to a page showing its source with covered, partially covered and uncovered lines
highlighted. CI uploads the same report as the `coverage-report` artifact. Coverage
accumulates across runs, so delete `*.gcda` files in the build directory (or use a fresh build
directory) before re-running `ctest` for a clean report.

## What is covered

### `can_leaf_ze1_parsing_tests` — `canLeafZE1.cpp`

Compiles `simppeliTCU/canLeafZE1.cpp` against the stubs in `test_canInterface_stubs.cpp`.
15 tests, about 85% of the lines in that file. Covered areas:

- Decoding of SOC (0x55B), cabin temperature (0x54F), car awake (0x601), charger (0x390),
  HVAC status (0x54B), HVAC setpoint (0x54A) and doors/locks (0x60D)
- Detecting the nag screen popup (0x628 / 0x62C / 0x683), including how it is deferred while
  reversing (0x421)
- Timing and sequence counter of the `DISMISS_NAG` send sequence
- Reassembling the multi-frame AV-CAN GPS message (0x62C)

### `can_interface_tests` / `can_interface_no_second_can_tests` — `canInterface.cpp`

Compile the real `simppeliTCU/canInterface.cpp` against the fakes in [`fakes/`](fakes/). It is
built twice: with `USE_SECOND_CAN=1` (LilyGO T-2Can default, 23 tests) and `USE_SECOND_CAN=0`
(18 tests). Both variants reach 100% line coverage of the file. Covered areas:

- `sendCAN`: frame contents (standard ID, DLC, data), TX timeout, `> ` / `! ` log prefix
- Frame log format, timestamps relative to `resetCanLogTimestamps()` (including `millis()`
  wraparound), dropping the log line when the Serial TX buffer is too full, DLC > 8 clamping
- `readAndHandleCANMessage`: dispatch to `handleReceivedMessage`, non-blocking receive, at most
  4 frames per call and bus, primary bus before second bus
- `setupCAN`: 500 kbit/s, board pins from `hardware.h`, RX queue 32, accept-all filter, and the
  success and failure messages
- Second CAN: MCP2515 pins, mirroring sent frames, failure reporting, `2< ` receive log; or,
  when disabled, that the second bus is never touched

| Fake | Replaces | Test controls / inspects |
| --- | --- | --- |
| `fakes/Arduino.h`, `fake_arduino.cpp` | `millis()`, `Serial` | `fakeMillis`; `Serial.output`, `Serial.writeCapacity` |
| `fakes/fake_can_drivers.*` | ESP-IDF TWAI driver | `fakeTwai`: install/start/transmit results, recorded configs and frames, `rxQueue` |
| | MCP2515 (`secondCan.h`) | `fakeSecondCan`: setup/send results, recorded pins and frames, `rxQueue` |
| | `handleReceivedMessage()` | `handledMessages` |

The TWAI types come from `host/shims/driver/twai.h`, so tests and the host application share one
definition. `fakes/` is first on the include path so `<Arduino.h>` resolves to the test fake
rather than the host shim.

## Test isolation

All `canLeafZE1.cpp` tests use the `CanLeafZE1Parsing` fixture. Its `SetUp()` calls `resetCanLeafZE1State()`,
which returns the internal state of `canLeafZE1.cpp` (nag-popup and reverse flags, AV sequence
counter, GPS reassembly buffer, send-sequence state machine, HVAC setpoint) to power-on values,
and `resetMocks()`, which clears the stub globals. The `CanInterface` fixture resets all fakes,
sets `fakeMillis` to 0 and calls `resetCanLogTimestamps()`. Every test therefore starts from the
same state.

`ctest` runs every suite with `--gtest_shuffle`, so a test that leaks state into another fails
in a random order. A failing run prints its seed; reproduce it with:

```bash
./can_leaf_ze1_parsing_tests --gtest_shuffle --gtest_random_seed=<seed>
```

`resetCanLeafZE1State()` only exists when `SIMPPELITCU_UNIT_TEST` is defined, which
`test/CMakeLists.txt` does for the test executable; firmware and host builds don't contain it.
When you add internal state to `canLeafZE1.cpp`, also reset it in `resetCanLeafZE1State()`.

## Known shortcomings

### 1. Most of the firmware is still not tested

These sources are not compiled into any test, so they get no tests and no coverage:

| File | Untested responsibility |
| --- | --- |
| `mqttInterface.cpp` | Topic building, command parsing, publish logic |
| `hassDiscovery.cpp` | Home Assistant discovery payloads, availability topics |
| `configuration.cpp` | Loading and saving preferences, defaults |
| `cliParser.cpp` | Serial CLI command parsing |
| `simppeliTCU.ino` | Real `handle*` callbacks (decoded value → MQTT publish), main loop |
| `secondCan.cpp` | Second CAN bus forwarding |
| `stringBuffer.h` | Bounded string helper (overflow and truncation behaviour) |

The `handle*` functions are replaced with stubs that only store the last value they received.
The tests therefore check *decoding* but not what the firmware *does* with a decoded value.

The Arduino/ESP32 shims in `host/shims/` already let all of these files build on Linux for the
host application. The `canInterface.cpp` tests show the pattern for the rest: compile the real
source against small fakes in `test/fakes/` that record calls and return configured results.

### 2. Gaps inside `canLeafZE1.cpp`

These paths never run (gcov, Debug build):

- **Wake-up phase of the sequence state machine:** `WAKING` / `WAKE_WAIT`, the 0x68C wake-up
  ping, `WAKE_SUCCESS`, and `WAKE_TIMEOUT` after 130 attempts. Only `DISMISS_NAG`, which skips
  wake-up, is tested.
- **All other sequences:** `REFRESH`, `HVAC_ON`, `HVAC_OFF`, `CHARGE_ON`, `LOCK_DOORS` and
  `UNLOCK_DOORS` (the frames and timing actually sent to the car).
- **`setHVACTargetTemperature()` and `hvacPayloadModifier`:** the setpoint byte injected into
  `HVAC_ON` frames is never checked.
- **Multi-repeat steps:** the step-interval branch (`repeats > 1`) never runs.
- **`decodeVentilationMode()`:** no test asserts a ventilation mode.
- **Signed field extraction:** the sign-extension path in `extractScaledValue`
  (`canInterface.h`).
- **Error paths:** an unknown charger status, `parse_av_can_gps(nullptr, …)`, a GPS
  consecutive frame that is too short, and `startSequence()` called while a sequence is active
  or with `CanSequence::NONE`.
- **Length checking:** no test confirms that frames shorter than the expected DLC are dropped
  by `handleReceivedMessage`.
- **`millis()` wraparound** in `manageCANSequence`'s time comparison.
- **GPS hemispheres:** only north/east coordinates are tested. Southern and western positions
  are untested, and the decoder has no hemisphere handling.

### 3. Test infrastructure issues

- `struct SentCANMessage` is defined twice, once in each test source file. The two definitions
  must stay identical by hand, or the program violates the One Definition Rule. Move it into a
  shared header.
- Most tests run several unrelated cases one after another. When one case fails, later
  cases can fail too and hide the real cause. Parameterised tests (`TEST_P`) would keep the
  cases independent.
- `CMakeLists.txt` lists header files as sources of the test executable (harmless, but
  noise).

### 4. CI and reporting

- The coverage report in CI (gcovr → Codecov) only contains files that the tests compile, which
  is just `canLeafZE1.cpp` and `canInterface.cpp`. The reported figure therefore looks much
  better than the real coverage of the firmware, because the untested files are left out
  instead of counted as 0%.
- CI has no minimum coverage threshold.
- The host application ships with `host/sample-can.log`, but CI never replays it, so nothing
  tests the complete path from CAN frame to MQTT message.
- `docs/compiling.md` only shows commands for MinGW on Windows.

### 5. Issues found in `canInterface.cpp` while writing its tests

- `sendCAN` copies `len` bytes into the 8-byte `twai_message_t::data` without clamping, so a
  caller passing `len > 8` overflows the buffer. Current callers never do; this is not tested
  because it is undefined behaviour.
- `readAndHandleCANMessage` passes the raw TWAI `data_length_code` (which can exceed 8) to
  `handleReceivedMessage`; only the log output is clamped.
