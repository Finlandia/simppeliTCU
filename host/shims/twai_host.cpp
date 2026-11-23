// Host backend for the TWAI driver API.
//   --can <iface>     : Linux SocketCAN (e.g. vcan0, or a real USB-CAN adapter as can0)
//   --replay <file>   : feed frames from a candump log (`candump -l` format)
//   (neither)         : sent frames are only logged, nothing is received
#include "driver/twai.h"

#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "Arduino.h"
#include "host_config.h"

namespace {

enum class Backend { NONE, SOCKETCAN, REPLAY };

struct ReplayFrame {
    unsigned long offsetMs;
    twai_message_t msg;
};

Backend backend = Backend::NONE;
int canFd = -1;
std::string canIface;

std::vector<ReplayFrame> replayFrames;
bool replayLoop = false;
size_t replayIndex = 0;
unsigned long replayStartMs = 0;
unsigned long replayPeriodMs = 0;
bool started = false;

// Parses one `candump -l` line: "(1700000000.123456) can0 55B#C800"
bool parseCandumpLine(const std::string& line, double& ts, twai_message_t& msg) {
    if (line.empty() || line[0] != '(') return false;
    char iface[32];
    char frame[64];
    if (sscanf(line.c_str(), "(%lf) %31s %63s", &ts, iface, frame) != 3) return false;

    const char* hash = strchr(frame, '#');
    if (!hash || hash[1] == '#' || hash[1] == 'R') return false;  // skip CAN FD and RTR frames

    memset(&msg, 0, sizeof(msg));
    size_t idLen = (size_t)(hash - frame);
    msg.identifier = (uint32_t)strtoul(std::string(frame, idLen).c_str(), nullptr, 16);
    msg.extd = idLen > 3 ? 1 : 0;

    const char* p = hash + 1;
    while (p[0] && p[1] && msg.data_length_code < 8) {
        if (*p == '.') { p++; continue; }
        char byteStr[3] = {p[0], p[1], 0};
        msg.data[msg.data_length_code++] = (uint8_t)strtoul(byteStr, nullptr, 16);
        p += 2;
    }
    return true;
}

}  // namespace

bool hostCanUseSocketCan(const char* iface) {
    backend = Backend::SOCKETCAN;
    canIface = iface;
    return true;
}

bool hostCanUseReplay(const char* path, bool loop) {
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "[host] Cannot open CAN replay file %s\n", path);
        return false;
    }
    std::string line;
    double firstTs = -1;
    while (std::getline(in, line)) {
        double ts;
        twai_message_t msg;
        if (!parseCandumpLine(line, ts, msg)) continue;
        if (firstTs < 0) firstTs = ts;
        replayFrames.push_back({(unsigned long)((ts - firstTs) * 1000.0 + 0.5), msg});
    }
    if (replayFrames.empty()) {
        fprintf(stderr, "[host] No frames found in %s (expected `candump -l` format)\n", path);
        return false;
    }
    // Wait at least one second between loop iterations so a short log does not flood the bus
    unsigned long duration = replayFrames.back().offsetMs;
    replayPeriodMs = duration + 100 > 1000 ? duration + 100 : 1000;
    replayLoop = loop;
    backend = Backend::REPLAY;
    fprintf(stderr, "[host] Loaded %zu CAN frames from %s%s\n", replayFrames.size(), path, loop ? " (looping)" : "");
    return true;
}

esp_err_t twai_driver_install(const twai_general_config_t*, const twai_timing_config_t*, const twai_filter_config_t*) {
    if (backend != Backend::SOCKETCAN) return ESP_OK;

    canFd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
    if (canFd < 0) {
        perror("[host] SocketCAN socket");
        return ESP_FAIL;
    }
    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, canIface.c_str(), IFNAMSIZ - 1);
    if (ioctl(canFd, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "[host] CAN interface %s not found. For a virtual bus run:\n"
                        "[host]   sudo modprobe vcan && sudo ip link add dev %s type vcan && sudo ip link set up %s\n",
                canIface.c_str(), canIface.c_str(), canIface.c_str());
        close(canFd);
        canFd = -1;
        return ESP_FAIL;
    }
    struct sockaddr_can addr = {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(canFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[host] SocketCAN bind");
        close(canFd);
        canFd = -1;
        return ESP_FAIL;
    }
    fprintf(stderr, "[host] CAN bound to SocketCAN interface %s\n", canIface.c_str());
    return ESP_OK;
}

esp_err_t twai_start() {
    started = true;
    replayStartMs = millis();
    replayIndex = 0;
    return (backend == Backend::SOCKETCAN && canFd < 0) ? ESP_FAIL : ESP_OK;
}

esp_err_t twai_get_status_info(twai_status_info_t* status_info) {
    *status_info = twai_status_info_t{};
    status_info->state = started ? TWAI_STATE_RUNNING : TWAI_STATE_STOPPED;
    return ESP_OK;
}

esp_err_t twai_transmit(const twai_message_t* message, TickType_t) {
    if (!started) return ESP_FAIL;
    if (backend != Backend::SOCKETCAN) return ESP_OK;  // no bus: pretend the frame went out
    if (canFd < 0) return ESP_FAIL;

    struct can_frame frame = {};
    frame.can_id = message->identifier | (message->extd ? CAN_EFF_FLAG : 0);
    frame.can_dlc = message->data_length_code > 8 ? 8 : message->data_length_code;
    memcpy(frame.data, message->data, frame.can_dlc);
    return ::write(canFd, &frame, sizeof(frame)) == (ssize_t)sizeof(frame) ? ESP_OK : ESP_FAIL;
}

esp_err_t twai_receive(twai_message_t* message, TickType_t) {
    if (!started) return ESP_ERR_TIMEOUT;

    if (backend == Backend::SOCKETCAN) {
        if (canFd < 0) return ESP_ERR_TIMEOUT;
        struct can_frame frame;
        ssize_t n = ::read(canFd, &frame, sizeof(frame));
        if (n != (ssize_t)sizeof(frame) || (frame.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG))) return ESP_ERR_TIMEOUT;
        memset(message, 0, sizeof(*message));
        message->extd = (frame.can_id & CAN_EFF_FLAG) ? 1 : 0;
        message->identifier = frame.can_id & (message->extd ? CAN_EFF_MASK : CAN_SFF_MASK);
        message->data_length_code = frame.can_dlc > 8 ? 8 : frame.can_dlc;
        memcpy(message->data, frame.data, message->data_length_code);
        return ESP_OK;
    }

    if (backend == Backend::REPLAY) {
        unsigned long now = millis();
        if (replayIndex >= replayFrames.size()) {
            if (!replayLoop || now - replayStartMs < replayPeriodMs) return ESP_ERR_TIMEOUT;
            replayStartMs += replayPeriodMs;
            replayIndex = 0;
        }
        const ReplayFrame& f = replayFrames[replayIndex];
        if (now - replayStartMs < f.offsetMs) return ESP_ERR_TIMEOUT;
        *message = f.msg;
        replayIndex++;
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}
