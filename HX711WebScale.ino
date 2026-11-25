#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include <Update.h>
#include "BoundedWebServer.h"
#include <WiFi.h>
#include "InventoryLogic.h"
#include "StorageHelpers.h"
#include "NetworkConfig.h"
#include <ESPmDNS.h>
#include "ScalePage.h"
#include "SettingsPage.h"
#include "WebCache.h"
#include "WebPages.generated.h"
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Replace these with your 2.4 GHz Wi-Fi network credentials.
const char *WIFI_SSID = "TP-LINK_123";
const char *WIFI_PASSWORD = "qhxq311601";

constexpr uint8_t HX711_DOUT_PIN = 6;
constexpr uint8_t HX711_SCK_PIN = 7;
constexpr uint8_t BUTTON_PIN = 3;
constexpr uint8_t TARE_SAMPLES = 10;
constexpr uint8_t RAW_WINDOW = 10;
constexpr uint32_t DEVICE_SETTINGS_VERSION = 2;
constexpr unsigned long SAMPLE_TIMEOUT_MS = 3000;
constexpr unsigned long SERIAL_REPORT_MS = 5000;
constexpr unsigned long STABLE_MS = 1500;
constexpr unsigned long EMPTY_CONFIRM_MS = 600;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 40;
constexpr unsigned long OTA_REBOOT_DELAY_MS = 1500;
constexpr uint32_t STORE_VERSION = 2;
constexpr uint32_t SETTINGS_VERSION = 1;
constexpr uint32_t CHECKPOINT_SETTINGS_VERSION = 1;

enum RunMode : uint8_t { SPOOL_MODE, SCALE_MODE };
enum SpoolState : uint8_t { NO_SPOOL, PENDING_SPOOL, ACTIVE_SPOOL };

struct InventoryStore {
  uint32_t version;
  uint32_t nextId;
  uint8_t count;
  inventory::SpoolRecord records[inventory::MAX_SPOOLS];
  uint32_t checksum;
};

struct PersistedSpoolSettings {
  uint32_t version;
  inventory::SpoolSettings values;
  uint32_t checksum;
};

struct PersistedCheckpointSettings {
  uint32_t version;
  inventory::CheckpointSettings values;
  uint32_t checksum;
};

struct PersistedSettings {
  uint32_t version;
  inventory::SpoolSettings spool;
  inventory::CheckpointSettings checkpoint;
  uint32_t checksum;
};

struct PersistedDeviceSettings {
  uint32_t version;
  inventory::SpoolSettings spool;
  inventory::CheckpointSettings checkpoint;
  network::Credentials wifi;
  uint32_t checksum;
};

HX711 scale;
Preferences preferences;
BoundedWebServer server(80);
InventoryStore inventoryStore = {};
bool inventoryWritable = true;
inventory::SpoolSettings spoolSettings;
inventory::CheckpointSettings checkpointSettings;
network::Credentials wifiConfig;

long offsetRaw = 0;
long latestRaw = 0;
float countsPerGram = 0.0f;
bool hasReading = false;
bool isTared = false;
bool webServerStarted = false;
RunMode runMode = SPOOL_MODE;
SpoolState spoolState = NO_SPOOL;
int activeIndex = -1;
long pendingRaw = 0;
long activeRaw = 0;
float activeTotal = 0;
bool loadDetected = false;
bool unloadPending = false;
long ordinaryTareRaw = 0;
long rawWindow[RAW_WINDOW] = {};
uint8_t rawCount = 0;
uint8_t rawNext = 0;
long stableCandidateRaw = 0;
long stableRaw = 0;
unsigned long stableCandidateSince = 0;
unsigned long emptySince = 0;
bool stableCandidateValid = false;
bool stableAvailable = false;
bool buttonLastRead = HIGH;
bool buttonStable = HIGH;
unsigned long buttonChangedAt = 0;
unsigned long otaRebootAt = 0;
unsigned long otaStartedAt = 0;
bool otaUploading = false;
bool otaSucceeded = false;
bool otaRebootPending = false;
String otaError;
unsigned long lastSampleAt = 0;
unsigned long lastWiFiAttempt = 0;
network::RecoveryPolicy networkRecovery;
bool fallbackAPActive = false;
bool stationWasConnected = false;
bool wifiChangePending = false;
bool mdnsStarted = false;
bool mdnsAttempted = false;
unsigned long lastMDNSAttemptAt = 0;
unsigned long wifiApplyRequestedAt = 0;
unsigned long lastAPAttemptAt = 0;
bool apAttempted = false;
char fallbackAPName[32] = {};
char deviceHostname[32] = {};
String settingsToken;
unsigned long lastSerialReportAt = 0;
unsigned long lastSpoolCheckpointAt = 0;


void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nHX711 Web Scale starting");
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonLastRead = buttonStable = digitalRead(BUTTON_PIN);
  Serial.printf("New-spool button: GPIO%u to GND\n", BUTTON_PIN);
  scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  Serial.printf("HX711 pins: DT=GPIO%u, SCK=GPIO%u\n", HX711_DOUT_PIN, HX711_SCK_PIN);
  preferences.begin("web-scale", false);
  offsetRaw = preferences.getLong("offset", 0);
  isTared = preferences.getBool("tared", false);
  countsPerGram = preferences.getFloat("factor", 0.0f);
  loadSettings();
  runMode = preferences.getUChar("mode", SPOOL_MODE) == SCALE_MODE ? SCALE_MODE : SPOOL_MODE;
  ordinaryTareRaw = offsetRaw;
  loadInventory();
  Serial.printf("Saved calibration: %s\n", isTared && fabsf(countsPerGram) >= 0.01f ? "ready" : "needed");
  Serial.printf("Mode: %s\n", runMode == SPOOL_MODE ? "spool" : "scale");

  const char *requestHeaders[] = {"Accept-Encoding", "If-None-Match", "X-Settings-Token", "If-None-Match"};
  server.collectHeaders(requestHeaders, 4);
  server.on("/", HTTP_GET, handleScalePage);
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/api/reading", HTTP_GET, handleReading);
  server.on("/api/spools", HTTP_GET, handleSpools);
  server.on("/api/spools/create", HTTP_POST, handleCreate);
  server.on("/api/spools/select", HTTP_POST, handleSelect);
  server.on("/api/spools/update", HTTP_POST, handleUpdate);
  server.on("/api/spools/delete", HTTP_POST, handleDelete);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/tare", HTTP_POST, handleTare);
  server.on("/api/base-zero", HTTP_POST, handleBaseZero);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsSave);
  server.on("/api/update", HTTP_POST, handleFirmwareResult, handleFirmwareUpload);

  setupNetwork();
}

void loop() {
  if (otaRebootPending && millis() - otaRebootAt >= OTA_REBOOT_DELAY_MS) ESP.restart();
  serviceNetwork();
  if (webServerStarted) {
    const unsigned long requestStartedAt = millis();
    server.handleClient();
    const unsigned long requestMs = millis() - requestStartedAt;
    if (requestMs > 1000) Serial.printf("Web request processing: %lu ms\n", requestMs);
  }


  serviceSensor();
  checkpointActiveSpool();

  const bool buttonRead = digitalRead(BUTTON_PIN);
  if (buttonRead != buttonLastRead) buttonChangedAt = millis();
  buttonLastRead = buttonRead;
  if (buttonRead != buttonStable && millis() - buttonChangedAt >= BUTTON_DEBOUNCE_MS) {
    buttonStable = buttonRead;
    if (buttonStable == LOW && !createNewSpool()) {
      Serial.println("New-spool button ignored: no stable unmatched load");
    }
  }

  if (millis() - lastSerialReportAt >= SERIAL_REPORT_MS) {
    lastSerialReportAt = millis();
    const bool available = sensorAvailable();
    if (!available) {
      Serial.println(" | HX711: no data");
    } else if (!calibrated()) {
      Serial.printf(" | HX711 raw=%ld | calibration needed\n", latestRaw);
    } else {
      const float grams = runMode == SCALE_MODE
                              ? inventory::gramsBetween(latestRaw, ordinaryTareRaw, countsPerGram)
                              : inventory::gramsBetween(latestRaw, offsetRaw, countsPerGram);
      Serial.printf(" | mode=%s | HX711 raw=%ld | weight=%.2f g | spool=%s\n",
                    runMode == SPOOL_MODE ? "spool" : "scale", latestRaw, grams,
                    spoolState == ACTIVE_SPOOL ? "active" : spoolState == PENDING_SPOOL ? "pending" : "empty");
    }
  }

  // Leave CPU time for Wi-Fi and the idle task on the single-core ESP32-C3.
  delay(2);
}
