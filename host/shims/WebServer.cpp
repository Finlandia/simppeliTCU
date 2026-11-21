#include "WebServer.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "host_config.h"

static std::string bindAddress = "127.0.0.1";
static int portOverride = 8080;

void hostWebServerConfigure(const char* address, int port) {
    if (address) bindAddress = address;
    portOverride = port;
}

static constexpr int REQUEST_TIMEOUT_MS = 1000;
static constexpr size_t MAX_REQUEST_SIZE = 16 * 1024;
static constexpr int MAX_CONNECTIONS_PER_POLL = 4;

WebServer::~WebServer() {
    if (listenFd_ >= 0) close(listenFd_);
}

void WebServer::begin() {
    int port = portOverride > 0 ? portOverride : port_;

    listenFd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenFd_ < 0) {
        perror("[host] WebServer socket");
        return;
    }
    int one = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, bindAddress.c_str(), &addr.sin_addr) != 1) {
        fprintf(stderr, "[host] Invalid bind address: %s\n", bindAddress.c_str());
        close(listenFd_);
        listenFd_ = -1;
        return;
    }
    if (bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(listenFd_, 8) < 0) {
        fprintf(stderr, "[host] Cannot listen on %s:%d: %s\n", bindAddress.c_str(), port, strerror(errno));
        close(listenFd_);
        listenFd_ = -1;
        return;
    }
    const char* shown = bindAddress == "0.0.0.0" ? "localhost" : bindAddress.c_str();
    fprintf(stderr, "[host] Web UI: http://%s:%d/\n", shown, port);
}

void WebServer::handleClient() {
    if (listenFd_ < 0) return;
    for (int i = 0; i < MAX_CONNECTIONS_PER_POLL; i++) {
        int fd = accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (fd < 0) return;  // EAGAIN: nothing pending
        handleConnection(fd);
        close(fd);
    }
}

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string urlDecode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '+') {
            out += ' ';
        } else if (in[i] == '%' && i + 2 < in.size() && hexValue(in[i + 1]) >= 0 && hexValue(in[i + 2]) >= 0) {
            out += (char)(hexValue(in[i + 1]) * 16 + hexValue(in[i + 2]));
            i += 2;
        } else {
            out += in[i];
        }
    }
    return out;
}

static void parseArgs(const std::string& query, std::vector<std::pair<std::string, std::string>>& args) {
    size_t pos = 0;
    while (pos <= query.size() && !query.empty()) {
        size_t amp = query.find('&', pos);
        std::string part = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        if (!part.empty()) {
            size_t eq = part.find('=');
            if (eq == std::string::npos) {
                args.emplace_back(urlDecode(part), "");
            } else {
                args.emplace_back(urlDecode(part.substr(0, eq)), urlDecode(part.substr(eq + 1)));
            }
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
}

bool WebServer::readRequest(int fd, std::string& method, std::string& target, std::string& body) {
    std::string req;
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;

    while (true) {
        if (headerEnd != std::string::npos && req.size() >= headerEnd + 4 + contentLength) break;

        struct pollfd pfd = {fd, POLLIN, 0};
        if (poll(&pfd, 1, REQUEST_TIMEOUT_MS) <= 0) return false;
        char buf[2048];
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        req.append(buf, (size_t)n);
        if (req.size() > MAX_REQUEST_SIZE) return false;

        if (headerEnd == std::string::npos) {
            headerEnd = req.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                // Look for a Content-Length header (case-insensitive) for form POSTs
                std::string headers = req.substr(0, headerEnd);
                for (auto& c : headers) c = (char)tolower((unsigned char)c);
                size_t cl = headers.find("\r\ncontent-length:");
                if (cl != std::string::npos) contentLength = strtoul(headers.c_str() + cl + 17, nullptr, 10);
                if (contentLength > MAX_REQUEST_SIZE) return false;
            }
        }
    }

    size_t lineEnd = req.find("\r\n");
    std::string requestLine = req.substr(0, lineEnd);
    size_t sp1 = requestLine.find(' ');
    size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
    method = requestLine.substr(0, sp1);
    target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
    body = req.substr(headerEnd + 4, contentLength);
    return true;
}

void WebServer::handleConnection(int fd) {
    std::string method, target, body;
    if (!readRequest(fd, method, target, body)) return;

    std::string path = target;
    args_.clear();
    size_t q = target.find('?');
    if (q != std::string::npos) {
        path = target.substr(0, q);
        parseArgs(target.substr(q + 1), args_);
    }
    if (method == "POST") parseArgs(body, args_);

    status_ = 0;
    contentType_.clear();
    headers_.clear();
    body_.clear();

    bool found = false;
    for (auto& route : routes_) {
        if (route.first == path) {
            route.second();
            found = true;
            break;
        }
    }
    if (!found) send(404, "text/plain", "Not found");

    fprintf(stderr, "[host] HTTP %s %s -> %d\n", method.c_str(), target.c_str(), status_);
    writeResponse(fd);
}

static const char* reasonPhrase(int code) {
    switch (code) {
        case 200: return "OK";
        case 302: return "Found";
        case 303: return "See Other";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "";
    }
}

void WebServer::writeResponse(int fd) {
    if (status_ == 0) status_ = 200;
    std::string out = "HTTP/1.1 " + std::to_string(status_) + " " + reasonPhrase(status_) + "\r\n";
    if (!contentType_.empty()) out += "Content-Type: " + contentType_ + "\r\n";
    out += headers_;
    out += "Content-Length: " + std::to_string(body_.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += body_;

    size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

bool WebServer::hasArg(const char* name) const {
    for (auto& a : args_)
        if (a.first == name) return true;
    return false;
}

String WebServer::arg(const char* name) const {
    for (auto& a : args_)
        if (a.first == name) return String(a.second);
    return String();
}

void WebServer::send(int code, const char* contentType, const String& content) {
    status_ = code;
    if (contentType) contentType_ = contentType;
    body_ = content.str();
}

void WebServer::sendHeader(const char* name, const char* value, bool first) {
    std::string line = std::string(name) + ": " + value + "\r\n";
    headers_ = first ? line + headers_ : headers_ + line;
}
