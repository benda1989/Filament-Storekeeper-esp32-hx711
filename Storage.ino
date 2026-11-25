#include "StorageHelpers.h"

void loadSpoolSettings() {
  PersistedSpoolSettings stored = {};
  if (readVersionedStore(preferences, "spoolCfg", stored, SETTINGS_VERSION) &&
      inventory::validSettings(stored.values)) {
    spoolSettings = stored.values;
    Serial.println("Spool settings loaded");
    return;
  }
  spoolSettings = inventory::SpoolSettings{};
  Serial.println("Spool settings: defaults");
}

void loadCheckpointSettings() {
  PersistedCheckpointSettings stored = {};
  if (readVersionedStore(preferences, "checkpointCfg", stored, CHECKPOINT_SETTINGS_VERSION) &&
      inventory::validCheckpointSettings(stored.values)) {
    checkpointSettings = stored.values;
    Serial.printf("Checkpoint settings loaded: %u min, %.1f g\n",
                  checkpointSettings.intervalMinutes, checkpointSettings.minimumChangeG);
    return;
  }
  checkpointSettings = inventory::CheckpointSettings{};
  Serial.println("Checkpoint settings: defaults (5 min, 2.0 g)");
}

void loadSettings() {
  PersistedDeviceSettings device = {};
  if (readVersionedStore(preferences, "deviceCfg", device, DEVICE_SETTINGS_VERSION) &&
      inventory::validSettings(device.spool) && inventory::validCheckpointSettings(device.checkpoint) &&
      network::validCredentials(device.wifi)) {
    spoolSettings = device.spool;
    checkpointSettings = device.checkpoint;
    wifiConfig = device.wifi;
    Serial.println("Device settings loaded (including Wi-Fi)");
    return;
  }
  // Retain existing sketch credentials when upgrading from older settings formats.
  wifiConfig = {};
  strncpy(wifiConfig.ssid, WIFI_SSID, sizeof(wifiConfig.ssid) - 1);
  strncpy(wifiConfig.password, WIFI_PASSWORD, sizeof(wifiConfig.password) - 1);
  if (!network::validCredentials(wifiConfig)) wifiConfig = {};
  PersistedSettings stored = {};
  if (readVersionedStore(preferences, "settingsCfg", stored, SETTINGS_VERSION) &&
      inventory::validSettings(stored.spool) &&
      inventory::validCheckpointSettings(stored.checkpoint)) {
    spoolSettings = stored.spool;
    checkpointSettings = stored.checkpoint;
    Serial.println("Unified settings loaded");
    return;
  }
  // Older firmware saved these groups separately. Migrate on the next Save.
  loadSpoolSettings();
  loadCheckpointSettings();
}

bool saveSettings(const inventory::SpoolSettings &spool,
                  const inventory::CheckpointSettings &checkpoint,
                  const network::Credentials &wifi) {
  if (!inventory::validSettings(spool) || !inventory::validCheckpointSettings(checkpoint) ||
      !network::validCredentials(wifi)) return false;
  PersistedDeviceSettings stored = {};
  stored.version = DEVICE_SETTINGS_VERSION;
  stored.spool = spool;
  stored.checkpoint = checkpoint;
  stored.wifi = wifi;
  stored.checksum = recordChecksum(stored);
  if (preferences.putBytes("deviceCfg", &stored, sizeof(stored)) != sizeof(stored)) return false;
  spoolSettings = spool;
  checkpointSettings = checkpoint;
  wifiConfig = wifi;
  return true;
}

bool validInventory(const InventoryStore &store) {
  if (store.version != STORE_VERSION || store.count > inventory::MAX_SPOOLS || store.nextId == 0) return false;
  for (uint8_t i = 0; i < store.count; ++i) {
    if (!inventory::validRecord(store.records[i]) || store.records[i].id >= store.nextId) return false;
    for (uint8_t j = 0; j < i; ++j) if (store.records[j].id == store.records[i].id) return false;
  }
  return true;
}

bool saveInventory() {
  if (runMode != SPOOL_MODE || !inventoryWritable || !validInventory(inventoryStore)) return false;
  inventoryStore.checksum = recordChecksum(inventoryStore);
  const size_t written = preferences.putBytes("recordsV2", &inventoryStore, sizeof(inventoryStore));
  if (written != sizeof(inventoryStore)) Serial.println("Inventory save failed");
  return written == sizeof(inventoryStore);
}

void loadInventory() {
  memset(&inventoryStore, 0, sizeof(inventoryStore));
  inventoryStore.version = STORE_VERSION;
  inventoryStore.nextId = 1;
  inventoryWritable = true;
  if (preferences.getBytesLength("recordsV2")) {
    if (readVersionedStore(preferences, "recordsV2", inventoryStore, STORE_VERSION) && validInventory(inventoryStore)) return;
    // Never replace an unreadable store with a fresh empty inventory.
    inventoryWritable = false;
  } else if (preferences.getBytesLength("records")) {
    struct LegacyStore {
      uint32_t version;
      uint32_t nextId;
      uint8_t count;
      inventory::LegacySpoolRecord records[inventory::MAX_SPOOLS];
      uint32_t checksum;
    } legacy = {};
    if (readVersionedStore(preferences, "records", legacy, 1) && legacy.count <= inventory::MAX_SPOOLS &&
        (legacy.count == 0 || (isTared && std::isfinite(countsPerGram) && std::fabs(countsPerGram) >= 0.01f))) {
      inventoryStore.nextId = legacy.nextId;
      inventoryStore.count = legacy.count;
      for (uint8_t i = 0; i < legacy.count; ++i) {
        const auto &old = legacy.records[i];
        auto &record = inventoryStore.records[i];
        record.id = old.id;
        record.total = inventory::gramsBetween(old.lastRaw, offsetRaw, countsPerGram);
        record.tare = old.flags & inventory::TARE_KNOWN
                          ? inventory::gramsBetween(old.tareRaw, offsetRaw, countsPerGram) : NAN;
        memcpy(record.color, old.color, sizeof(record.color));
        record.material = old.material;
      }
      if (validInventory(inventoryStore)) return;  // Persist v2 only on a subsequent save.
    }
    inventoryWritable = false;
  } else {
    return;
  }
  memset(&inventoryStore, 0, sizeof(inventoryStore));
  inventoryStore.version = STORE_VERSION;
  inventoryStore.nextId = 1;
  Serial.println("Inventory unreadable: legacy data retained, writes blocked");
}
