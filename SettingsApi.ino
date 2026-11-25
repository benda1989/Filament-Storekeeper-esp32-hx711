void performTare(bool baseZero) {
  long mean;
  if (!averageRaw(TARE_SAMPLES, mean)) {
    Serial.println("Tare failed: HX711 not responding");
    server.send(503, "text/plain; charset=utf-8", "HX711 无响应，请检查接线");
    return;
  }
  if (!baseZero) {
    ordinaryTareRaw = mean;
    Serial.printf("Ordinary tare: raw=%ld\n", mean);
    server.send(200, "text/plain; charset=utf-8", "OK");
    return;
  }
  if (calibrated() &&
      inventory::classify(inventory::gramsBetween(mean, offsetRaw, countsPerGram), spoolSettings) != inventory::EMPTY) {
    server.send(409, "text/plain; charset=utf-8", "请先清空称台再归零");
    return;
  }
  offsetRaw = mean;
  isTared = true;
  preferences.putLong("offset", offsetRaw);
  preferences.putBool("tared", true);
  ordinaryTareRaw = offsetRaw;
  rawCount = 0;
  stableCandidateValid = false;
  stableAvailable = false;
  resetSpoolDetection();
  Serial.printf("Tare complete: offset=%ld\n", offsetRaw);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

void handleTare() { performTare(runMode == SPOOL_MODE); }
void handleBaseZero() { performTare(true); }

void handleCalibrate() {
  if (!isTared) {
    server.send(400, "text/plain; charset=utf-8", "请先去皮");
    return;
  }
  const String input = server.arg("grams");
  char *end = nullptr;
  const float knownGrams = strtof(input.c_str(), &end);
  if (input.isEmpty() || end == input.c_str() || *end != '\0' ||
      !isfinite(knownGrams) || knownGrams <= 0 || knownGrams > 1000000) {
    server.send(400, "text/plain; charset=utf-8", "请输入有效的砝码重量（克）");
    return;
  }
  long mean;
  if (!averageRaw(TARE_SAMPLES, mean)) {
    Serial.println("Calibration failed: HX711 not responding");
    server.send(503, "text/plain; charset=utf-8", "HX711 无响应，请检查接线");
    return;
  }
  const float factor = (mean - offsetRaw) / knownGrams;
  if (fabsf(factor) < 0.01f) {
    server.send(400, "text/plain; charset=utf-8", "读数变化太小，请检查砝码和接线");
    return;
  }
  countsPerGram = factor;
  preferences.putFloat("factor", countsPerGram);
  rawCount = 0;
  stableCandidateValid = false;
  stableAvailable = false;
  resetSpoolDetection();
  Serial.printf("Calibration complete: %.2f g, %.4f counts/g\n", knownGrams, countsPerGram);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

String jsonQuoted(const char *value) {
  String result = "\"";
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
    if (*p == '\\' || *p == '"') { result += '\\'; result += static_cast<char>(*p); }
    else if (*p < 0x20) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
      result += escaped;
    } else result += static_cast<char>(*p);
  }
  result += '"';
  return result;
}

void handleSettingsGet() {
  char numbers[300];
  snprintf(numbers, sizeof(numbers),
           "{\"calibrated\":%s,\"nominalG\":%.1f,\"matchToleranceG\":%.1f,"
           "\"emptyLimitG\":%.1f,\"loadedLimitG\":%.1f,\"intervalMinutes\":%u,\"minimumChangeG\":%.1f,",
           calibrated() ? "true" : "false", spoolSettings.nominalG,
           spoolSettings.matchToleranceG, spoolSettings.emptyLimitG, spoolSettings.loadedLimitG,
           checkpointSettings.intervalMinutes, checkpointSettings.minimumChangeG);
  String json = numbers;
  json += "\"wifiSsid\":"; json += jsonQuoted(wifiConfig.ssid);
  json += ",\"wifiOpen\":"; json += (wifiConfig.ssid[0] && !wifiConfig.password[0]) ? "true" : "false";
  json += ",\"settingsToken\":"; json += jsonQuoted(settingsToken.c_str());
  json += ",\"apSsid\":"; json += jsonQuoted(fallbackAPName);
  json += ",\"apUrl\":\"http://192.168.4.1\",\"deviceUrl\":\"http://";
  json += deviceHostname; json += ".local/settings\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

bool parseWiFiSettings(network::Credentials &proposed) {
  proposed = wifiConfig;
  if (!server.hasArg("wifiSsid")) return true;  // Older API clients can retain Wi-Fi settings.
  const String ssid = server.arg("wifiSsid");
  const String password = server.arg("wifiPassword");
  const String open = server.arg("wifiOpen");
  if (ssid.length() > 32 || password.length() > 64 ||
      strlen(ssid.c_str()) != ssid.length() || strlen(password.c_str()) != password.length() ||
      (open != "true" && open != "false")) return false;
  if (ssid.isEmpty()) return !wifiConfig.ssid[0] && password.isEmpty();
  const bool sameSsid = strcmp(ssid.c_str(), wifiConfig.ssid) == 0;
  memset(proposed.ssid, 0, sizeof(proposed.ssid));
  memcpy(proposed.ssid, ssid.c_str(), ssid.length());
  if (open == "true") {
    if (!password.isEmpty()) return false;
    memset(proposed.password, 0, sizeof(proposed.password));
  } else if (!password.isEmpty()) {
    memset(proposed.password, 0, sizeof(proposed.password));
    memcpy(proposed.password, password.c_str(), password.length());
  } else if (!sameSsid || !wifiConfig.password[0]) {
    return false;
  }
  return network::validCredentials(proposed);
}

bool parseSettingValue(const char *name, float &value) {
  const String input = server.arg(name);
  char *end = nullptr;
  value = strtof(input.c_str(), &end);
  return !input.isEmpty() && end != input.c_str() && *end == '\0' && isfinite(value);
}

void handleSettingsSave() {
  server.sendHeader("Cache-Control", "no-store");
  if (settingsToken.isEmpty() || server.header("X-Settings-Token") != settingsToken) {
    server.send(403, "text/plain; charset=utf-8", "页面已失效，请重新打开设置页");
    return;
  }
  if (wifiChangePending || otaUploading || otaRebootPending) {
    server.send(409, "text/plain; charset=utf-8", "设备正在切换网络或更新固件，请稍后重试");
    return;
  }
  network::Credentials proposedWiFi;
  if (!parseWiFiSettings(proposedWiFi)) {
    server.send(400, "text/plain; charset=utf-8", "Wi-Fi 参数无效；新网络须填写密码，或选择无密码网络");
    return;
  }
  const bool networkChanged = !network::sameCredentials(proposedWiFi, wifiConfig);
  inventory::SpoolSettings spool;
  inventory::CheckpointSettings checkpoint;
  float minutes;
  if (!parseSettingValue("nominalG", spool.nominalG) ||
      !parseSettingValue("matchToleranceG", spool.matchToleranceG) ||
      !parseSettingValue("emptyLimitG", spool.emptyLimitG) ||
      !parseSettingValue("loadedLimitG", spool.loadedLimitG) ||
      !parseSettingValue("intervalMinutes", minutes) ||
      !parseSettingValue("minimumChangeG", checkpoint.minimumChangeG) ||
      minutes < 1 || minutes > 120 || floorf(minutes) != minutes) {
    server.send(400, "text/plain; charset=utf-8", "请填写有效参数；保存间隔须为 1–120 的整数");
    return;
  }
  checkpoint.intervalMinutes = static_cast<uint16_t>(minutes);
  if (!inventory::validSettings(spool) || !inventory::validCheckpointSettings(checkpoint)) {
    server.send(400, "text/plain; charset=utf-8", "参数超出范围；上料阈值须至少比空台阈值高 20 g");
    return;
  }
  if (spoolState == ACTIVE_SPOOL && calibrated() &&
      inventory::classify(inventory::gramsBetween(activeRaw, offsetRaw, countsPerGram), spool) != inventory::LOADED) {
    server.send(409, "text/plain; charset=utf-8", "当前耗材低于新的上料阈值；请取下耗材后再调整");
    return;
  }
  if (!saveSettings(spool, checkpoint, proposedWiFi)) {
    Serial.println("Settings save failed; previous values retained");
    server.send(507, "text/plain; charset=utf-8", "设置保存失败，原设置未改变，请重试");
    return;
  }
  if (spoolState != ACTIVE_SPOOL) resetSpoolDetection();
  Serial.printf("Settings saved: nominal=%.1f g, match=%.1f g, empty=%.1f g, loaded=%.1f g, checkpoint=%u min/%.1f g\n",
                spool.nominalG, spool.matchToleranceG, spool.emptyLimitG, spool.loadedLimitG,
                checkpoint.intervalMinutes, checkpoint.minimumChangeG);
  server.send(200, "application/json; charset=utf-8",
              networkChanged ? "{\"networkChanged\":true}" : "{\"networkChanged\":false}");
  if (networkChanged) scheduleWiFiApply();
}
