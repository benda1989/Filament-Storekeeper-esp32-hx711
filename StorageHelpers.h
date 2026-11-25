#pragma once
#include <cstddef>
#include <cstdint>

inline uint32_t checksumBytes(const void *data, size_t length) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}

template <typename Record>
uint32_t recordChecksum(const Record &record) {
  return checksumBytes(&record, offsetof(Record, checksum));
}

// Validate the stored envelope; callers retain record-specific value checks.
template <typename Record, typename Storage>
bool readVersionedStore(Storage &storage, const char *key, Record &record, uint32_t version) {
  return storage.getBytesLength(key) == sizeof(record) &&
         storage.getBytes(key, &record, sizeof(record)) == sizeof(record) &&
         record.version == version && record.checksum == recordChecksum(record);
}
