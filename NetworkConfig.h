#pragma once
#include <cstdint>
#include <cstring>

namespace network {
struct Credentials {
  char ssid[33] = {};
  char password[65] = {};
};
inline bool validCredentials(const Credentials &value) {
  const auto *ssidEnd = static_cast<const char *>(std::memchr(value.ssid, 0, sizeof(value.ssid)));
  const auto *passwordEnd = static_cast<const char *>(std::memchr(value.password, 0, sizeof(value.password)));
  if (!ssidEnd || !passwordEnd) return false;
  const size_t ssidLength = ssidEnd - value.ssid, length = passwordEnd - value.password;
  if (!ssidLength) return !length;  // Unconfigured: AP is immediately available.
  if (!length || (length >= 8 && length <= 63)) return true;
  if (length != 64) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value.password[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
inline bool sameCredentials(const Credentials &a, const Credentials &b) {
  return std::strcmp(a.ssid, b.ssid) == 0 && std::strcmp(a.password, b.password) == 0;
}
constexpr uint32_t AP_FALLBACK_MS = 30000;
constexpr uint32_t CONNECTED_STABLE_MS = 10000;
constexpr uint32_t RETRY_MS = 30000;
constexpr uint32_t APPLY_DELAY_MS = 1500;
struct RecoveryAction { bool startAP = false; bool stopAP = false; };
class RecoveryPolicy {
  uint32_t disconnectedAt = 0, connectedAt = 0;
  bool watching = false, wasConnected = false;
public:
  void reset(uint32_t now) { disconnectedAt = now; watching = true; wasConnected = false; }
  RecoveryAction tick(uint32_t now, bool connected, bool configured, bool apActive) {
    if (connected) {
      if (!wasConnected) connectedAt = now;
      wasConnected = true;
      return {false, apActive && uint32_t(now - connectedAt) >= CONNECTED_STABLE_MS};
    }
    if (!watching || wasConnected) { disconnectedAt = now; watching = true; }
    wasConnected = false;
    return {!apActive && (!configured || uint32_t(now - disconnectedAt) >= AP_FALLBACK_MS), false};
  }
};
}
