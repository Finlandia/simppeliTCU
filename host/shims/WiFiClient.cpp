#include "WiFiClient.h"
#include "WiFiClientSecure.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

static constexpr int CONNECT_TIMEOUT_MS = 3000;

// Non-blocking connect with a timeout, so an unreachable broker does not freeze the loop for minutes.
static int connectWithTimeout(const struct addrinfo* ai) {
    int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (rc < 0 && errno == EINPROGRESS) {
        struct pollfd pfd = {fd, POLLOUT, 0};
        rc = poll(&pfd, 1, CONNECT_TIMEOUT_MS);
        if (rc == 1) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
            rc = err == 0 ? 0 : -1;
        } else {
            rc = -1;
        }
    }
    if (rc != 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, flags);  // back to blocking for writes
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return fd;
}

int WiFiClient::connect(IPAddress ip, uint16_t port) { return connect(ip.toString().c_str(), port); }

int WiFiClient::connect(const char* host, uint16_t port) {
    stop();
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return 0;

    for (struct addrinfo* ai = res; ai && fd_ < 0; ai = ai->ai_next) {
        fd_ = connectWithTimeout(ai);
    }
    freeaddrinfo(res);
    return fd_ >= 0 ? 1 : 0;
}

size_t WiFiClient::write(const uint8_t* buf, size_t size) {
    if (fd_ < 0) return 0;
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(fd_, buf + sent, size - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            stop();
            break;
        }
        sent += (size_t)n;
    }
    return sent;
}

int WiFiClient::available() {
    if (fd_ < 0) return 0;
    int n = 0;
    if (ioctl(fd_, FIONREAD, &n) < 0) return 0;
    return n;
}

int WiFiClient::read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
}

int WiFiClient::read(uint8_t* buf, size_t size) {
    if (fd_ < 0) return -1;
    ssize_t n = recv(fd_, buf, size, MSG_DONTWAIT);
    if (n == 0) {
        stop();
        return -1;
    }
    return n < 0 ? -1 : (int)n;
}

int WiFiClient::peek() {
    if (fd_ < 0) return -1;
    uint8_t c;
    return recv(fd_, &c, 1, MSG_PEEK | MSG_DONTWAIT) == 1 ? c : -1;
}

void WiFiClient::stop() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

uint8_t WiFiClient::connected() {
    if (fd_ < 0) return 0;
    uint8_t c;
    ssize_t n = recv(fd_, &c, 1, MSG_PEEK | MSG_DONTWAIT);
    if (n > 0) return 1;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 1;
    stop();  // orderly shutdown by peer, or a socket error
    return 0;
}

int WiFiClientSecure::connect(const char*, uint16_t) {
    static bool warned = false;
    if (!warned) {
        fprintf(stderr,
                "[host] TLS is not supported in the host build. Use a plain MQTT broker:\n"
                "[host]   set mqtt_tls false\n"
                "[host]   set mqtt_port 1883\n");
        warned = true;
    }
    return 0;
}
