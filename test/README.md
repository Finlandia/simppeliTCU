# Unit tests

Host-side GoogleTest suites for the firmware. Build and run instructions are in
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

### Basic suites for the remaining sources

Every other firmware source has its own test executable, so all of `simppeliTCU/` shows up in the
coverage report. These tests are deliberately basic: a few smoke tests per file, meant as a
starting point to extend. They are compiled against the host-application shims (`host/shims/`)
and the real PubSubClient library, with the same flags as `host/CMakeLists.txt` (shared through
the `host_shims` library in `CMakeLists.txt`).

| Executable | Source under test | Lines | What the basic tests check |
| --- | --- | --- | --- |
| `string_buffer_tests` | `stringBuffer.h` | 75% | Truncation, format/append, trim, lower-case, compare helpers |
| `configuration_tests` | `configuration.cpp` | 91% | Defaults, persistence across reload, length/hostname/AP-password validation, factory reset |
| `cli_parser_tests` | `cliParser.cpp` | 86% | `get`, `set`, `list` (secrets hidden), errors, one line read from stdin |
| `mqtt_interface_tests` | `mqttInterface.cpp` | 57% | OVMS command dispatch, lock PIN ignored, foreign topics ignored, updates while disconnected |
| `hass_discovery_tests` | `hassDiscovery.cpp` | 4% | Metric topic names; nothing is published without a broker connection |
| `second_can_tests` | `secondCan.cpp` | 100% | Setup (pins, 500 kbit/s, failure), send clamps to 8 bytes, receive |
| `ui_tests` | `ui.h` | 100% | Web page: placeholders, value formatting, charger/ventilation labels, location link, auto-refresh, HVAC details, setpoint default, lock buttons |
| `sketch_tests` | `simppeliTCU.ino` | 15% | CAN callbacks store values, fan speed 0 ignored during HVAC, GPS only with tracking enabled |

Line percentages are from the combined report of all suites.

Notes:

- Tests that use configuration call `useFreshConfiguration()` from `host_test_helpers.h`. It
  points the Preferences shim at an empty temporary file, so every test starts from the defaults
  and no settings file is left behind.
- `second_can_tests` is built with `USE_SECOND_CAN=1`. The MCP2515 library and SPI are faked in
  `fakes/second_can/`.
- `sketch_tests` compiles the sketch through `host/sketch.cpp` and links all other firmware
  sources, like the host application. `setup()` and `loop()` are not called: they would start
  WiFi, the web server and MQTT.
- `ui_tests` renders the page into the host shim's `WebServer` and reads the response back
  through `responseStatus()`, `responseContentType()` and `responseBody()`. These accessors are
  declared only when `SIMPPELITCU_UNIT_TEST` is defined, so the host application doesn't have them.
- `cli_parser_tests` never runs `reboot` or `factory-reset`, because `ESP.restart()` re-executes
  the process in the host shim.

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

### 1. Several sources have only smoke tests

Every source now has a test executable, but the basic suites leave most behaviour untested:

| File | Main gaps |
| --- | --- |
| `hassDiscovery.cpp` | Discovery payloads are never checked: publishing needs a connected PubSubClient. A fake `Client` that completes the MQTT CONNECT handshake would allow capturing what is published. |
| `mqttInterface.cpp` | `manageMQTT()`, reconnecting, the Last Will, and the topics and payloads actually published (same reason as above) |
| `simppeliTCU.ino` | `setup()`, `loop()`, WiFi management, the web UI handlers (`handleRoot`, `handleHvacOn`, …), and starting sequences from MQTT commands |
| `cliParser.cpp` | `reboot` and `factory-reset` (they restart the process) |

The tests check that `handle*` callbacks in the sketch store decoded values. They don't yet check
what is published to MQTT.

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

- CI has no minimum coverage threshold.
- The host-shim based suites build only on Unix (Linux, WSL, macOS), because the shims use
  POSIX sockets and `poll`. On Windows/MinGW only the CAN suites are built.
- The host application ships with `host/sample-can.log`, but CI never replays it, so nothing
  tests the complete path from CAN frame to MQTT message.
- `docs/compiling.md` only shows commands for MinGW on Windows.

### 5. Issues found in `canInterface.cpp` while writing its tests

- `sendCAN` copies `len` bytes into the 8-byte `twai_message_t::data` without clamping, so a
  caller passing `len > 8` overflows the buffer. Current callers never do; this is not tested
  because it is undefined behaviour.
- `readAndHandleCANMessage` passes the raw TWAI `data_length_code` (which can exceed 8) to
  `handleReceivedMessage`; only the log output is clamped.
