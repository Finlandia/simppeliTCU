// Small single-threaded HTTP/1.1 server with the subset of the ESP32 WebServer API
// that the firmware uses. Each response is buffered and sent with Connection: close.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "WString.h"

#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

class WebServer {
public:
    using Handler = std::function<void()>;

    explicit WebServer(int port = 80) : port_(port) {}
    ~WebServer();

    void on(const char* path, Handler handler) { routes_.emplace_back(path, std::move(handler)); }
    void begin();
    void handleClient();

    bool hasArg(const char* name) const;
    String arg(const char* name) const;

    void send(int code, const char* contentType = "text/plain", const String& content = String());
    void sendHeader(const char* name, const char* value, bool first = false);
    void setContentLength(size_t) {}
    void sendContent(const String& content) { body_ += content.str(); }
    void sendContent(const char* content) { if (content) body_ += content; }

#ifdef SIMPPELITCU_UNIT_TEST
    // Unit-test builds only: the response built by the last handler call
    int responseStatus() const { return status_; }
    const std::string& responseContentType() const { return contentType_; }
    const std::string& responseBody() const { return body_; }
#endif

private:
    void handleConnection(int fd);
    bool readRequest(int fd, std::string& method, std::string& target, std::string& body);
    void writeResponse(int fd);

    int port_;
    int listenFd_ = -1;
    std::vector<std::pair<std::string, Handler>> routes_;
    std::vector<std::pair<std::string, std::string>> args_;

    // Response being built by the current handler
    int status_ = 0;
    std::string contentType_;
    std::string headers_;
    std::string body_;
};
