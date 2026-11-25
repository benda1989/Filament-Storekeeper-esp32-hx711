bool calibrated() {
  return isTared && isfinite(countsPerGram) && fabsf(countsPerGram) >= 0.01f;
}

bool sensorReadFailed = false;
unsigned long sensorWaitMs = 0;

bool sensorAvailable() {
  return hasReading && !sensorReadFailed;
}

float totalGrams(long raw) { return inventory::gramsBetween(raw, offsetRaw, countsPerGram); }

bool platformLoaded() { return calibrated() && sensorAvailable() && loadDetected; }

uint8_t readingState() {
  if (!sensorAvailable()) return 98;
  if (!calibrated()) return 99;
  return loadDetected ? 1 : 0;
}

uint32_t confirmedId() {
  return runMode == SPOOL_MODE && calibrated() && loadDetected && !unloadPending &&
         spoolState == ACTIVE_SPOOL && activeIndex >= 0
             ? inventoryStore.records[activeIndex].id : 0;
}

bool persistConsumedWeight() {
  if (activeIndex < 0) return true;
  auto &record = inventoryStore.records[activeIndex];
  const float previous = record.total;
  const float next = inventory::consumedTotal(previous, activeTotal);
  if (next == previous) return true;
  record.total = next;
  if (saveInventory()) return true;
  record.total = previous;
  return false;
}

// A failed final save is retried before another spool may acquire an identity.
void flushUnloadedSpool() {
  if (!unloadPending || !persistConsumedWeight()) return;
  unloadPending = false;
  activeIndex = -1;
}

void releaseSpool() {
  if (spoolState == ACTIVE_SPOOL && activeIndex >= 0) unloadPending = true;
  spoolState = NO_SPOOL;
  pendingRaw = 0;
  flushUnloadedSpool();
}

void checkpointActiveSpool() {
  if (runMode != SPOOL_MODE) return;
  if (unloadPending) { flushUnloadedSpool(); return; }
  if (!confirmedId() || !platformLoaded() || !stableAvailable) return;
  const unsigned long now = millis();
  const unsigned long intervalMs = static_cast<unsigned long>(checkpointSettings.intervalMinutes) * 60000UL;
  if (now - lastSpoolCheckpointAt < intervalMs) return;
  if (inventory::checkpointDue(now, lastSpoolCheckpointAt, intervalMs, activeTotal,
                              inventoryStore.records[activeIndex].total, checkpointSettings.minimumChangeG)) {
    if (!persistConsumedWeight()) return;
  }
  lastSpoolCheckpointAt = now;
}

void clearStability() {
  rawCount = 0;
  rawNext = 0;
  stableCandidateValid = false;
  stableAvailable = false;
}

void resetSpoolDetection() {
  releaseSpool();
  loadDetected = false;
  emptySince = 0;
  clearStability();
}

void updateLoadState() {
  if (!calibrated()) { loadDetected = false; return; }
  const auto weightClass = inventory::classify(totalGrams(latestRaw), spoolSettings);
  if (weightClass == inventory::LOADED) {
    loadDetected = true;
    emptySince = 0;
  } else if (weightClass == inventory::EMPTY) {
    if (!emptySince) emptySince = millis();
    if (millis() - emptySince >= EMPTY_CONFIRM_MS) {
      if (loadDetected) { releaseSpool();clearStability(); }
      loadDetected = false;
    }
  } else {
    emptySince = 0;  // Hysteresis retains the last physical load state.
  }
}

void processStableRaw(long raw) {
  if (runMode != SPOOL_MODE || !platformLoaded() || unloadPending || inventory::classify(totalGrams(raw), spoolSettings) != inventory::LOADED) return;
  if (spoolState == ACTIVE_SPOOL) {
    activeRaw = raw;
    activeTotal = inventory::consumedTotal(activeTotal, totalGrams(raw));
    return;
  }
  const int match = inventory::uniqueMatch(inventoryStore.records, inventoryStore.count, totalGrams(raw), spoolSettings);
  if (match >= 0) {
    if (!activateSpool(match, raw, false)) Serial.println("Spool confirmation save failed; will retry");
  } else {
    spoolState = PENDING_SPOOL;
    pendingRaw = raw;
  }
}

void pushRaw(long raw) {
  rawWindow[rawNext] = raw;
  rawNext = (rawNext + 1) % RAW_WINDOW;
  if (rawCount < RAW_WINDOW) ++rawCount;
  if (rawCount < RAW_WINDOW || !calibrated()) { stableAvailable = false; return; }
  long minimum = rawWindow[0], maximum = rawWindow[0];
  int64_t sum = 0;
  for (uint8_t i = 0; i < RAW_WINDOW; ++i) {
    if (rawWindow[i] < minimum) minimum = rawWindow[i];
    if (rawWindow[i] > maximum) maximum = rawWindow[i];
    sum += rawWindow[i];
  }
  if (fabsf(inventory::gramsBetween(maximum, minimum, countsPerGram)) > 3.0f) {
    stableCandidateValid = false;
    stableAvailable = false;
    return;
  }
  const long mean = static_cast<long>(sum / RAW_WINDOW);
  if (!stableCandidateValid || fabsf(inventory::gramsBetween(mean, stableCandidateRaw, countsPerGram)) > 2.0f) {
    stableCandidateRaw = mean;
    stableCandidateSince = millis();
    stableCandidateValid = true;
    stableAvailable = false;
    return;
  }
  if (millis() - stableCandidateSince >= STABLE_MS) {
    stableRaw = mean;
    stableAvailable = true;
    processStableRaw(mean);
  }
}

// Sensor availability and measurement stability do not erase a confirmed identity.
void acceptRaw(long raw) {
  if (sensorReadFailed || (hasReading && millis() - lastSampleAt >= SAMPLE_TIMEOUT_MS)) { clearStability(); emptySince = 0; }
  latestRaw = raw;
  hasReading = true;
  lastSampleAt = millis();
  sensorReadFailed = false;
  sensorWaitMs = 0;
  updateLoadState();
  pushRaw(raw);
}

void markSensorReadFailure() {
  sensorReadFailed = true;
  clearStability();
  emptySince = 0;
}

// Count only time actually spent waiting for HX711, never time spent serving HTTP.
void serviceSensor() {
  const unsigned long start = millis();
  while (!scale.is_ready()) {
    if (millis() - start >= 2) {
      const unsigned long waited = millis() - start;
      if (waited >= SAMPLE_TIMEOUT_MS - sensorWaitMs) {
        sensorWaitMs = SAMPLE_TIMEOUT_MS;
        markSensorReadFailure();
      } else {
        sensorWaitMs += waited;
      }
      return;
    }
    delay(1);
  }
  acceptRaw(scale.read());
}

bool averageRaw(uint8_t samples, long &result) {
  int64_t sum = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    const unsigned long start = millis();
    while (!scale.is_ready()) {
      if (millis() - start >= SAMPLE_TIMEOUT_MS) { markSensorReadFailure(); return false; }
      delay(1);
    }
    sum += scale.read();
  }
  result = static_cast<long>(sum / samples);
  latestRaw = result;
  hasReading = true;
  lastSampleAt = millis();
  sensorReadFailed = false;
  sensorWaitMs = 0;
  return true;
}
