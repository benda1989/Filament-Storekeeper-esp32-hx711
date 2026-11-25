#pragma once
#include <WebServer.h>
#include <lwip/sockets.h>
#include <cerrno>

// Native WebServer owns all HTTP parsing, including segmented POST bodies.
// Serve one connection at a time; an unused preconnection yields to queued work.
class BoundedWebServer : public WebServer {
public:
  explicit BoundedWebServer(int port = 80, unsigned long sendMs = 2000)
      : WebServer(port), sendMs_(sendMs) {}

  void handleClient() override {
    if (_currentStatus == HC_NONE) {
      _currentClient = _server.accept();
      if (!_currentClient) return;
      _currentStatus = HC_WAIT_READ;
      _statusChange = millis();
    }
    if (_currentStatus == HC_WAIT_READ && !_currentClient.available() && _server.hasClient()) {
      _currentClient.stop();
      _currentClient = NetworkClient();
      _currentStatus = HC_NONE;
      _currentUpload.reset();
      _currentRaw.reset();
      return;
    }
    sending_ = false;
    WebServer::handleClient();
  }

protected:
  size_t _currentClientWrite(const char *data, size_t size) override { return sendBounded(data, size); }
  size_t _currentClientWrite_P(PGM_P data, size_t size) override { return sendBounded(data, size); }

private:
  unsigned long sendMs_, sendStartedAt_ = 0;
  bool sending_ = false;

  size_t sendBounded(const char *data, size_t size) {
    if (!sending_) { sending_ = true; sendStartedAt_ = millis(); }
    size_t sent = 0;
    while (sent < size && _currentClient.fd() >= 0) {
      if (millis() - sendStartedAt_ >= sendMs_) {
        Serial.println("HTTP connection dropped: send deadline");
        _currentClient.stop();
        break;
      }
      const int fd = _currentClient.fd();
      const int count = ::send(fd, data + sent, size - sent, MSG_DONTWAIT);
      if (count > 0) { sent += count; continue; }
      if (count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        _currentClient.stop();
        break;
      }
      fd_set writable;
      FD_ZERO(&writable); FD_SET(fd, &writable);
      timeval wait = {0, 20000};
      select(fd + 1, nullptr, &writable, nullptr, &wait);
      delay(1);
    }
    return sent;
  }
};
