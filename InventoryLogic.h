#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace inventory {

constexpr uint8_t MAX_SPOOLS = 32;
constexpr uint8_t TARE_KNOWN = 1;

struct SpoolSettings {
  float nominalG = 1000.0f;
  float matchToleranceG = 5.0f;
  float emptyLimitG = 20.0f;
  float loadedLimitG = 80.0f;
};

struct CheckpointSettings {
  uint16_t intervalMinutes = 5;
  float minimumChangeG = 2.0f;
};

inline bool validCheckpointSettings(const CheckpointSettings &settings) {
  return settings.intervalMinutes >= 1 && settings.intervalMinutes <= 120 &&
         std::isfinite(settings.minimumChangeG) &&
         settings.minimumChangeG >= 0.5f && settings.minimumChangeG <= 100.0f;
}

inline bool validSettings(const SpoolSettings &settings) {
  return std::isfinite(settings.nominalG) && settings.nominalG >= 100.0f && settings.nominalG <= 5000.0f &&
         std::isfinite(settings.matchToleranceG) && settings.matchToleranceG >= 0.5f && settings.matchToleranceG <= 50.0f &&
         std::isfinite(settings.emptyLimitG) && settings.emptyLimitG >= 1.0f && settings.emptyLimitG <= 100.0f &&
         std::isfinite(settings.loadedLimitG) && settings.loadedLimitG >= settings.emptyLimitG + 20.0f &&
         settings.loadedLimitG <= 500.0f;
}

enum Material : uint8_t { PLA, PETG, ABS, TPU, OTHER };
enum WeightClass : uint8_t { EMPTY, TRANSITION, LOADED };

// Persisted v2 weights are grams, independent of the current ADC calibration.
struct SpoolRecord {
  uint32_t id;
  float tare;  // NaN marks an unknown tare, including a total below nominal net weight.
  float total;
  char color[8];
  uint8_t material;
};

// Kept byte-for-byte for read-only conversion of the original records key.
struct LegacySpoolRecord {
  uint32_t id;
  int32_t tareRaw;
  int32_t lastRaw;
  char color[8];
  char nfcUid[20];
  uint8_t material;
  uint8_t flags;
};

inline bool validRecord(const SpoolRecord &record) {
  if (!record.id || record.material > OTHER || record.color[0] != '#' || record.color[7] != 0 ||
      !std::isfinite(record.total) || record.total < 0 || record.total > 1000000 ||
      !(std::isnan(record.tare) || (std::isfinite(record.tare) && record.tare >= 0 && record.tare <= 1000000))) return false;
  for (uint8_t i = 1; i < 7; ++i) {
    const char c = record.color[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

inline void moveToFront(SpoolRecord *records, uint8_t count, uint8_t index) {
  if (index == 0 || index >= count) return;
  const SpoolRecord used = records[index];
  std::memmove(records + 1, records, index * sizeof(SpoolRecord));
  records[0] = used;
}

inline float gramsBetween(int32_t reading, int32_t reference, float countsPerGram) {
  return (reading - reference) / countsPerGram;
}

// Consumption never increases a stored weight. Unknown identities never call this.
inline bool checkpointDue(uint32_t now, uint32_t lastAt, uint32_t intervalMs,
                          float current, float saved, float minimumChangeG) {
  return now - lastAt >= intervalMs && std::isfinite(current) && current >= 0 &&
         saved - current >= minimumChangeG;
}

inline float consumedTotal(float saved, float measured) {
  return std::isfinite(measured) && measured >= 0 ? std::fmin(saved, measured) : saved;
}

inline WeightClass classify(float grossGrams, const SpoolSettings &settings) {
  if (std::fabs(grossGrams) <= settings.emptyLimitG) return EMPTY;
  if (grossGrams >= settings.loadedLimitG) return LOADED;
  return TRANSITION;
}

// Weight alone cannot identify two similar spools reliably.
inline int uniqueMatch(const SpoolRecord *records, uint8_t count, float total,
                       const SpoolSettings &settings) {
  int found = -1;
  for (uint8_t i = 0; i < count; ++i) {
    const float difference = std::fabs(total - records[i].total);
    if (difference <= settings.matchToleranceG) {
      if (found >= 0) return -1;
      found = i;
    }
  }
  return found;
}

}  // namespace inventory
