// Host backend for the two CAN interfaces of the firmware:
//   bus 0: ESP32 TWAI driver API         (EV-CAN on the car)
//   bus 1: MCP2515 library API, over SPI (AV-CAN on the car: GPS, nag screen dismissal)
//
// Each bus is backed by one of:
//   --can <iface> / --can2 <iface> : Linux SocketCAN (e.g. vcan0/vcan1, or real adapters)
//   --replay <file>                : a `candump -l` log. Frames logged on the --replay-can2
//                                    interface name (default vcan1) go to bus 1, all others to bus 0.
//   (neither)                      : sent frames are only logged, nothing is received
#include "driver/twai.h"
#include "mcp2515.h"

#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

#include "Arduino.h"
#include "host_config.h"

SPIClass SPI;

namespace {

constexpr int BUS_COUNT = 2;
constexpr size_t REPLAY_QUEUE_MAX = 256;  // per bus; oldest frames are dropped beyond this

enum class Backend { NONE, SOCKETCAN, REPLAY };

struct HostFrame {
    uint32_t id;
    bool extd;
    uint8_t len;
    uint8_t data[8];
};

struct ReplayFrame {
    unsigned long offsetMs;
    int bus;
    HostFrame frame;
};

struct Bus {
    Backend backend = Backend::NONE;
    std::string iface;
    int fd = -1;
    bool started = false;
    std::deque<HostFrame> rxQueue;  // replay frames that are due but not read yet
};

Bus buses[BUS_COUNT];

std::vector<ReplayFrame> replayFrames;
bool replayLoop = false;
bool replayStarted = false;
size_t replayIndex = 0;
unsigned long replayStartMs = 0;
unsigned long replayPeriodMs = 0;
std::string replayCan2Iface = "vcan1";

const char* busName(int bus) { return bus == 0 ? "CAN (TWAI)" : "second CAN (MCP2515)"; }

// Parses one `candump -l` line: "(1700000000.123456) can0 55B#C800"
bool parseCandumpLine(const std::string& line, double& ts, std::string& iface, HostFrame& f) {
    if (line.empty() || line[0] != '(') return false;
    char ifaceBuf[32];
    char frame[64];
    if (sscanf(line.c_str(), "(%lf) %31s %63s", &ts, ifaceBuf, frame) != 3) return false;
    iface = ifaceBuf;

    const char* hash = strchr(frame, '#');
    if (!hash || hash[1] == '#' || hash[1] == 'R') return false;  // skip CAN FD and RTR frames

    memset(&f, 0, sizeof(f));
    size_t idLen = (size_t)(hash - frame);
    f.id = (uint32_t)strtoul(std::string(frame, idLen).c_str(), nullptr, 16);
    f.extd = idLen > 3;

    const char* p = hash + 1;
    while (p[0] && p[1] && f.len < 8) {
        if (*p == '.') { p++; continue; }
        char byteStr[3] = {p[0], p[1], 0};
        f.data[f.len++] = (uint8_t)strtoul(byteStr, nullptr, 16);
        p += 2;
    }
    return true;
}

// Moves every replay frame that is due into its bus queue. Both buses share one timeline,
// so frames keep their relative order and timing across the two interfaces.
void replayAdvance() {
    if (!replayStarted) {
        replayStarted = true;
        replayStartMs = millis();
        replayIndex = 0;
    }
    unsigned long now = millis();
    for (;;) {
        if (replayIndex >= replayFrames.size()) {
            if (!replayLoop || now - replayStartMs < replayPeriodMs) return;
            replayStartMs += replayPeriodMs;
            replayIndex = 0;
        }
        const ReplayFrame& f = replayFrames[replayIndex];
        if (now - replayStartMs < f.offsetMs) return;
        std::deque<HostFrame>& q = buses[f.bus].rxQueue;
        if (q.size() >= REPLAY_QUEUE_MAX) q.pop_front();
        q.push_back(f.frame);
        replayIndex++;
    }
}

bool socketCanOpen(Bus& bus, int index) {
    bus.fd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
    if (bus.fd < 0) {
        perror("[host] SocketCAN socket");
        return false;
    }
    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, bus.iface.c_str(), IFNAMSIZ - 1);
    if (ioctl(bus.fd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "[host] CAN interface %s not found. For a virtual bus run:\n"
                        "[host]   sudo modprobe vcan && sudo ip link add dev %s type vcan && sudo ip link set up %s\n",
                bus.iface.c_str(), bus.iface.c_str(), bus.iface.c_str());
        close(bus.fd);
        bus.fd = -1;
        return false;
    }
    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(bus.fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[host] SocketCAN bind");
        close(bus.fd);
        bus.fd = -1;
        return false;
    }
    fprintf(stderr, "[host] %s bound to SocketCAN interface %s\n", busName(index), bus.iface.c_str());
    return true;
}

bool busOpen(int index) {
    Bus& bus = buses[index];
    if (bus.backend == Backend::SOCKETCAN && bus.fd < 0 && !socketCanOpen(bus, index)) return false;
    return true;
}

bool busSend(int index, const HostFrame& f) {
    Bus& bus = buses[index];
    if (!bus.started) return false;
    if (bus.backend != Backend::SOCKETCAN) return true;  // no bus: pretend the frame went out
    if (bus.fd < 0) return false;

    struct can_frame frame = {};
    frame.can_id = f.id | (f.extd ? CAN_EFF_FLAG : 0);
    frame.can_dlc = f.len > 8 ? 8 : f.len;
    memcpy(frame.data, f.data, frame.can_dlc);
    return ::write(bus.fd, &frame, sizeof(frame)) == (ssize_t)sizeof(frame);
}

bool busReceive(int index, HostFrame& f) {
    Bus& bus = buses[index];
    if (!bus.started) return false;

    if (bus.backend == Backend::SOCKETCAN) {
        if (bus.fd < 0) return false;
        struct can_frame frame;
        ssize_t n = ::read(bus.fd, &frame, sizeof(frame));
        if (n != (ssize_t)sizeof(frame) || (frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG))) return false;
        memset(&f, 0, sizeof(f));
        f.extd = (frame.can_id & CAN_EFF_FLAG) != 0;
        f.id = frame.can_id & (f.extd ? CAN_EFF_MASK : CAN_SFF_MASK);
        f.len = frame.can_dlc > 8 ? 8 : frame.can_dlc;
        memcpy(f.data, frame.data, f.len);
        return true;
    }

    if (bus.backend == Backend::REPLAY) {
        replayAdvance();
        if (bus.rxQueue.empty()) return false;
        f = bus.rxQueue.front();
        bus.rxQueue.pop_front();
        return true;
    }

    return false;
}

}  // namespace

// --- Host configuration (main.cpp) -----------------------------------------------------

bool hostCanUseSocketCan(int bus, const char* iface) {
    if (bus < 0 || bus >= BUS_COUNT) return false;
    buses[bus].backend = Backend::SOCKETCAN;
    buses[bus].iface = iface;
    return true;
}

void hostCanSetReplayCan2Iface(const char* iface) { replayCan2Iface = iface; }

bool hostCanUseReplay(const char* path, bool loop) {
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "[host] Cannot open CAN replay file %s\n", path);
        return false;
    }
    std::string line;
    double firstTs = -1;
    size_t perBus[BUS_COUNT] = {0, 0};
    while (std::getline(in, line)) {
        double ts;
        std::string iface;
        HostFrame f;
        if (!parseCandumpLine(line, ts, iface, f)) continue;
        if (firstTs < 0) firstTs = ts;
        int bus = iface == replayCan2Iface ? 1 : 0;
        perBus[bus]++;
        replayFrames.push_back({(unsigned long)((ts - firstTs) * 1000.0 + 0.5), bus, f});
    }
    if (replayFrames.empty()) {
        fprintf(stderr, "[host] No frames found in %s (expected `candump -l` format)\n", path);
        return false;
    }
    // Wait at least one second between loop iterations so a short log does not flood the bus
    unsigned long duration = replayFrames.back().offsetMs;
    replayPeriodMs = duration + 100 > 1000 ? duration + 100 : 1000;
    replayLoop = loop;
    for (Bus& bus : buses) bus.backend = Backend::REPLAY;
    fprintf(stderr, "[host] Loaded %zu CAN frames from %s%s: %zu on CAN, %zu on second CAN (%s)\n",
            replayFrames.size(), path, loop ? " (looping)" : "", perBus[0], perBus[1], replayCan2Iface.c_str());
    return true;
}

// --- ESP-IDF TWAI driver API (bus 0) ---------------------------------------------------

esp_err_t twai_driver_install(const twai_general_config_t*, const twai_timing_config_t*, const twai_filter_config_t*) {
    return busOpen(0) ? ESP_OK : ESP_FAIL;
}

esp_err_t twai_start() {
    buses[0].started = true;
    return (buses[0].backend == Backend::SOCKETCAN && buses[0].fd < 0) ? ESP_FAIL : ESP_OK;
}

esp_err_t twai_transmit(const twai_message_t* message, TickType_t) {
    HostFrame f = {};
    f.id = message->identifier;
    f.extd = message->extd;
    f.len = message->data_length_code > 8 ? 8 : message->data_length_code;
    memcpy(f.data, message->data, f.len);
    return busSend(0, f) ? ESP_OK : ESP_FAIL;
}

esp_err_t twai_receive(twai_message_t* message, TickType_t) {
    HostFrame f;
    if (!busReceive(0, f)) return ESP_ERR_TIMEOUT;
    memset(message, 0, sizeof(*message));
    message->identifier = f.id;
    message->extd = f.extd;
    message->data_length_code = f.len;
    memcpy(message->data, f.data, f.len);
    return ESP_OK;
}

esp_err_t twai_get_status_info(twai_status_info_t* status_info) {
    *status_info = twai_status_info_t{};
    status_info->state = buses[0].started ? TWAI_STATE_RUNNING : TWAI_STATE_STOPPED;
    return ESP_OK;
}

// --- autowp-mcp2515 library API (bus 1) ------------------------------------------------

MCP2515::ERROR MCP2515::reset() {
    Bus& bus = buses[1];
    bus.started = false;
    bus.rxQueue.clear();
    return busOpen(1) ? ERROR_OK : ERROR_FAIL;
}

MCP2515::ERROR MCP2515::setBitrate(CAN_SPEED) {
    return (buses[1].backend == Backend::SOCKETCAN && buses[1].fd < 0) ? ERROR_FAIL : ERROR_OK;
}

MCP2515::ERROR MCP2515::setNormalMode() {
    buses[1].started = true;
    return ERROR_OK;
}

MCP2515::ERROR MCP2515::sendMessage(const struct can_frame* frame) {
    HostFrame f = {};
    f.extd = (frame->can_id & CAN_EFF_FLAG) != 0;
    f.id = frame->can_id & (f.extd ? CAN_EFF_MASK : CAN_SFF_MASK);
    f.len = frame->can_dlc > 8 ? 8 : frame->can_dlc;
    memcpy(f.data, frame->data, f.len);
    return busSend(1, f) ? ERROR_OK : ERROR_FAILTX;
}

MCP2515::ERROR MCP2515::readMessage(struct can_frame* frame) {
    HostFrame f;
    if (!busReceive(1, f)) return ERROR_NOMSG;
    memset(frame, 0, sizeof(*frame));
    frame->can_id = f.id | (f.extd ? CAN_EFF_FLAG : 0);
    frame->can_dlc = f.len;
    memcpy(frame->data, f.data, f.len);
    return ERROR_OK;
}
