#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace webcache {
constexpr uint32_t sourceHash(const char *data, size_t size) {
  uint32_t result = 2166136261u;
  for (size_t i = 0; i < size; ++i) result = (result ^ static_cast<uint8_t>(data[i])) * 16777619u;
  return result;
}
inline std::string_view trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
  return value;
}
inline bool equalCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const char c = a[i] >= 'A' && a[i] <= 'Z' ? a[i] + ('a' - 'A') : a[i];
    if (c != b[i]) return false;
  }
  return true;
}
inline bool acceptsGzip(std::string_view header) {
  bool wildcard = false;
  while (!header.empty()) {
    const size_t comma = header.find(',');
    const auto part = trim(header.substr(0, comma));
    const auto encoding = trim(part.substr(0, part.find(';')));
    bool allowed = true;
    auto params = part.find(';') == std::string_view::npos ? std::string_view{} : part.substr(part.find(';') + 1);
    while (!params.empty()) {
      const size_t semi = params.find(';');
      const auto parameter = trim(params.substr(0, semi));
      const size_t equals = parameter.find('=');
      if (equals != std::string_view::npos && equalCase(trim(parameter.substr(0, equals)), "q")) {
        const auto value = trim(parameter.substr(equals + 1));
        char number[16] = {};
        if (value.empty() || value.size() >= sizeof(number)) allowed = false;
        else {
          memcpy(number, value.data(), value.size());
          char *end = nullptr;
          const float quality = strtof(number, &end);
          allowed = end != number && *end == '\0' && quality > 0 && quality <= 1;
        }
      }
      if (semi == std::string_view::npos) break;
      params.remove_prefix(semi + 1);
    }
    if (equalCase(encoding, "gzip")) return allowed;
    if (encoding == "*") wildcard = allowed;
    if (comma == std::string_view::npos) break;
    header.remove_prefix(comma + 1);
  }
  return wildcard;
}
inline bool etagMatches(std::string_view header, std::string_view etag) {
  while (!header.empty()) {
    const size_t comma = header.find(',');
    auto tag = trim(header.substr(0, comma));
    if (tag == "*") return true;
    if (tag.substr(0, 2) == "W/") tag.remove_prefix(2);
    if (tag == etag) return true;
    if (comma == std::string_view::npos) break;
    header.remove_prefix(comma + 1);
  }
  return false;
}
}
