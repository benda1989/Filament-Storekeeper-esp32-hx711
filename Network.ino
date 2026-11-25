bool stationConnected() {
  return WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0 &&
         WiFi.SSID() == wifiConfig.ssid;
}

void startWebIfReady() {
  if (!webServerStarted && (stationConnected() || fallbackAPActive)) {
    server.begin();
    webServerStarted = true;
    Serial.println("HTTP server started");
  }
  if (!mdnsStarted && (stationConnected() || fallbackAPActive) &&
      (!mdnsAttempted || millis() - lastMDNSAttemptAt >= 5000)) {
    mdnsAttempted = true;
    lastMDNSAttemptAt = millis();
    mdnsStarted = MDNS.begin(deviceHostname);
    if (mdnsStarted) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("Device address: http://%s.local\n", deviceHostname);
    }
  }
}

bool startFallbackAP() {
  if (fallbackAPActive) return true;
  const unsigned long now = millis();
  if (apAttempted && now - lastAPAttemptAt < 5000) return false;
  apAttempted = true;
  lastAPAttemptAt = now;
  // AP+STA keeps the recovery page available while trying router credentials.
  if (!WiFi.mode(WIFI_AP_STA) ||
      !WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP(fallbackAPName)) {
    Serial.println("Fallback AP start failed; retrying later");
    return false;
  }
  fallbackAPActive = true;
  Serial.printf("Fallback AP: %s (open), http://192.168.4.1\n", fallbackAPName);
  startWebIfReady();
  return true;
}

void connectStation() {
  WiFi.disconnect(false, false);
  if (wifiConfig.ssid[0]) {
    WiFi.begin(wifiConfig.ssid, wifiConfig.password);
    Serial.println("Wi-Fi: connecting (2.4 GHz)");
  }
  lastWiFiAttempt = millis();
}

void setupNetwork() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // Explicit retries keep the AP recovery schedule predictable.
  const unsigned long suffix = static_cast<unsigned long>((ESP.getEfuseMac() >> 24) & 0xffffff);
  snprintf(fallbackAPName, sizeof(fallbackAPName), "Scale-%06lx", suffix);
  snprintf(deviceHostname, sizeof(deviceHostname), "scale-%06lx", suffix);
  WiFi.setHostname(deviceHostname);
  char token[33];
  snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
  settingsToken = token;
  Serial.printf("Wi-Fi power save: %s\n", WiFi.setSleep(false) ? "disabled" : "disable failed");
  networkRecovery.reset(millis());
  connectStation();
  if (!wifiConfig.ssid[0]) startFallbackAP();
}

void scheduleWiFiApply() {
  wifiApplyRequestedAt = millis();
  wifiChangePending = true;
}

void serviceNetwork() {
  const unsigned long now = millis();
  // Send the settings response first and never interrupt an ongoing OTA write.
  if (wifiChangePending && now - wifiApplyRequestedAt >= network::APPLY_DELAY_MS &&
      !otaUploading && !otaRebootPending && startFallbackAP()) {
    wifiChangePending = false;
    stationWasConnected = false;
    networkRecovery.reset(now);
    connectStation();
  }
  const bool connected = stationConnected();
  if (connected && !stationWasConnected) {
    Serial.printf("Wi-Fi connected: http://%s | BSSID=%s | channel=%d | RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.BSSIDstr().c_str(), WiFi.channel(), WiFi.RSSI());
  } else if (!connected && stationWasConnected) {
    Serial.println("Wi-Fi disconnected; fallback AP after 30 seconds");
  }
  stationWasConnected = connected;
  const network::RecoveryAction action = networkRecovery.tick(now, connected, wifiConfig.ssid[0] != 0, fallbackAPActive);
  if (!otaUploading && !otaRebootPending) {
    if (action.startAP) startFallbackAP();
    if (action.stopAP && !wifiChangePending) {
      if (WiFi.softAPdisconnect(true)) {
        fallbackAPActive = false;
        apAttempted = false;
        Serial.println("Wi-Fi stable; fallback AP closed");
      }
    }
    if (!connected && !wifiChangePending && wifiConfig.ssid[0] && millis() - lastWiFiAttempt >= network::RETRY_MS) {
      connectStation();
    }
  }
  startWebIfReady();
}
