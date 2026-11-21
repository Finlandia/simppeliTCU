# simppeliTCU host build

This folder builds the firmware as an ordinary Linux program, `simppeliTCU_host`. It runs the real
`setup()`/`loop()` and the firmware sources unchanged. A small set of shims in [shims/](shims/) replaces the
Arduino/ESP32 APIs:

| Firmware uses               | Host replacement                                                        |
|-----------------------------|-------------------------------------------------------------------------|
| `WebServer` (port 80)       | HTTP server on `127.0.0.1:8080` (`--port`, `--bind`)                    |
| `WiFi`, mDNS, NetBIOS       | Always "connected", via the PC's own network                            |
| `WiFiClient` + PubSubClient | Real TCP sockets and the real PubSubClient library, to any MQTT broker  |
| `WiFiClientSecure` (TLS)    | **Not supported.** Use plain MQTT (`mqtt_tls false`)                    |
| `Preferences` (NVS)         | `key=value` text file, `./simppeliTCU_host_prefs.txt` (`--prefs`)       |
| `Serial`                    | stdout, and stdin for the CLI (`list`, `set ...`, `reboot`)             |
| TWAI CAN driver             | SocketCAN interface (`--can vcan0`) or a candump log replay (`--replay`)|
| MCP2515 second CAN          | Disabled (`USE_SECOND_CAN=0`)                                           |

## Build

```sh
cd host
cmake -S . -B build
cmake --build build -j
```

CMake downloads PubSubClient v2.8 from GitHub. To use a copy you already have instead:
`cmake -S . -B build -DPUBSUBCLIENT_DIR=$HOME/Arduino/libraries/PubSubClient`

## Run

```sh
./build/simppeliTCU_host --replay sample-can.log --loop
```

Then open http://localhost:8080/. Under WSL2 the page also opens from a Windows browser at the same
address.

The sample log runs a discharge and charge cycle that lasts about 61 s:

| Time        | What the firmware sees                                                              |
|-------------|-------------------------------------------------------------------------------------|
| 0–0.2 s     | Car awake, cabin 21.5 °C, HVAC off with setpoint 21 °C, all doors closed and locked |
| 0.25–30.25 s| Discharging: SOC drops from 90 % to 30 %, 1 % every 0.5 s. Charger idle             |
| 30.75–60.75 s | AC charging (220 V): SOC rises from 30 % back to 90 %, 1 % every 0.5 s            |
| 61.25 s     | Charging finished at 90 %                                                           |

The awake (`0x601`) frame repeats every 0.5 s. With `--loop` the cycle starts again right after the
last frame. The SOC and charging state show up on the web page and, when MQTT is configured, are
published as they change.

The log also contains `0x5C5` frames with an odometer that counts up from 123456 km while
discharging. The firmware does not decode `0x5C5` yet, so this value is not shown anywhere.

The first run creates the settings file with host-friendly defaults: an SSID is set so the web server
starts, and MQTT uses plain TCP on port 1883. Change settings by typing CLI commands into the terminal:

```
set mqtt_server localhost
set vehicle_id leaf1
reboot
```

Like the device, the firmware makes its first MQTT connection attempt about 30 s after boot.

### Live CAN with SocketCAN

```sh
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0
./build/simppeliTCU_host --can vcan0

# in another terminal (can-utils)
candump vcan0                 # see what the TCU sends
canplayer -I my-leaf.log vcan0=can0   # replay a recording from the car
cansend vcan0 55B#C800        # SOC 80 %
```

The stock WSL2 kernel may not include the `vcan` module. If so, use `--replay` instead.
A USB-CAN adapter that shows up as `can0` also works with `--can can0`.

## Notes

- `sketch.cpp` compiles `simppeliTCU.ino` as C++. The Arduino IDE generates function prototypes for
  `.ino` files automatically; a plain compiler does not. If a new sketch function is called before
  it is defined, add its prototype to `sketch.cpp`.
- Use `--bind 0.0.0.0` to reach the UI from other machines. It has no authentication, so only do this
  on a trusted network.
