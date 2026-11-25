void handleReading() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Scale-Mode", runMode == SPOOL_MODE ? "spool" : "scale");
  const uint8_t state = readingState();
  char value[32] = "null";
  if (hasReading && calibrated()) snprintf(value, sizeof(value), "%.1f", totalGrams(latestRaw));
  char json[140];
  snprintf(json, sizeof(json), "{\"raw\":%ld,\"state\":%u,\"id\":%lu,\"value\":%s}",
           latestRaw, state, static_cast<unsigned long>(confirmedId()), value);
  server.send(200, "application/json; charset=utf-8", json);
}

void handleSpools() {
  if (!inventoryWritable) {
    server.send(503, "text/plain; charset=utf-8", "耗材存储未能读取，旧数据已保留，请先排查存储或校准兼容性");
    return;
  }
  char tag[24];
  snprintf(tag, sizeof(tag), "\"spools-%08lx\"", static_cast<unsigned long>(recordChecksum(inventoryStore)));
  server.sendHeader("Cache-Control", "private, no-cache");
  server.sendHeader("ETag", tag);
  if (server.header("If-None-Match") == tag) {
    server.send(304, "application/json; charset=utf-8", "");
    return;
  }
  String json = "[";
  json.reserve(3800);
  for (uint8_t i = 0; i < inventoryStore.count; ++i) {
    const auto &record = inventoryStore.records[i];
    char tare[32] = "null";
    if (isfinite(record.tare)) snprintf(tare, sizeof(tare), "%.1f", record.tare);
    char item[180];
    snprintf(item, sizeof(item), "%s{\"id\":%lu,\"material\":%u,\"color\":\"%s\",\"tare\":%s,\"total\":%.1f}",
             i ? "," : "", static_cast<unsigned long>(record.id), record.material, record.color, tare, record.total);
    json += item;
  }
  json += ']';
  server.send(200, "application/json; charset=utf-8", json);
}

int findRecord(uint32_t id) {
  for (uint8_t i = 0; i < inventoryStore.count; ++i) if (inventoryStore.records[i].id == id) return i;
  return -1;
}

bool activateSpool(int index, long raw, bool synchronize) {
  if (runMode != SPOOL_MODE || !inventoryWritable || index < 0 || index >= inventoryStore.count || unloadPending) return false;
  const InventoryStore previous = inventoryStore;
  if (spoolState == ACTIVE_SPOOL && activeIndex >= 0 && activeIndex != index) {
    inventoryStore.records[activeIndex].total = inventory::consumedTotal(inventoryStore.records[activeIndex].total, activeTotal);
  }
  auto &record = inventoryStore.records[index];
  const float total = totalGrams(raw);
  record.total = synchronize ? total : inventory::consumedTotal(record.total, total);
  inventory::moveToFront(inventoryStore.records, inventoryStore.count, index);
  if (!saveInventory()) { inventoryStore = previous; return false; }
  activeIndex = 0;
  activeRaw = raw;
  activeTotal = inventoryStore.records[0].total;
  spoolState = ACTIVE_SPOOL;
  pendingRaw = 0;
  lastSpoolCheckpointAt = millis();
  return true;
}

bool parsePositiveId(uint32_t &id) {
  const String input = server.arg("id");
  char *end = nullptr;
  const unsigned long long parsed = strtoull(input.c_str(), &end, 10);
  if (input.isEmpty() || input[0] == '-' || end == input.c_str() || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) return false;
  id = static_cast<uint32_t>(parsed);
  return true;
}

bool parseNonnegativeGrams(const String &input, float &grams) {
  char *end = nullptr;
  grams = strtof(input.c_str(), &end);
  return !input.isEmpty() && end != input.c_str() && *end == '\0' && isfinite(grams) && grams >= 0 && grams <= 1000000;
}

int requestedRecord() {
  uint32_t id;
  if (!parsePositiveId(id)) { server.send(400, "text/plain; charset=utf-8", "无效的耗材 ID");return -1; }
  const int index = findRecord(id);
  if (index < 0) server.send(404, "text/plain; charset=utf-8", "找不到该耗材");
  return index;
}

bool createNewSpool() {
  if (runMode != SPOOL_MODE || !inventoryWritable || unloadPending || !stableAvailable || !platformLoaded() || confirmedId() ||
      inventory::classify(totalGrams(stableRaw), spoolSettings) != inventory::LOADED ||
      inventoryStore.count >= inventory::MAX_SPOOLS || inventoryStore.nextId == UINT32_MAX) return false;
  const InventoryStore previous = inventoryStore;
  inventory::SpoolRecord record = {};
  record.id = inventoryStore.nextId++;
  record.total = totalGrams(stableRaw);
  record.tare = record.total >= spoolSettings.nominalG ? record.total - spoolSettings.nominalG : NAN;
  strcpy(record.color, "#808080");
  record.material = inventory::OTHER;
  memmove(&inventoryStore.records[1], inventoryStore.records, inventoryStore.count * sizeof(record));
  inventoryStore.records[0] = record;
  ++inventoryStore.count;
  if (!saveInventory()) { inventoryStore = previous;return false; }
  activeIndex = 0;
  activeRaw = stableRaw;
  activeTotal = record.total;
  spoolState = ACTIVE_SPOOL;
  lastSpoolCheckpointAt = millis();
  return true;
}

bool requireSpoolMode() {
  if (runMode == SPOOL_MODE) return true;
  server.send(409, "text/plain; charset=utf-8", "普通模式不写入耗材记录，请先切换耗材模式");
  return false;
}

void handleCreate() {
  if (!requireSpoolMode()) return;
  if (createNewSpool()) {
    server.send(200, "text/plain; charset=utf-8", std::isnan(inventoryStore.records[activeIndex].tare)
        ? "已添加耗材；总重小于初始净重，空盘重量待确认，请编辑空盘重量"
        : "OK");
  } else {
    server.send(409, "text/plain; charset=utf-8", "请等待稳定的未识别耗材，并检查记录容量或存储状态");
  }
}

void handleSelect() {
  if (!requireSpoolMode()) return;
  uint32_t id;
  if (!stableAvailable || !platformLoaded() || unloadPending ||
      inventory::classify(totalGrams(stableRaw), spoolSettings) != inventory::LOADED || !parsePositiveId(id)) {
    server.send(409, "text/plain; charset=utf-8", "请先稳定耗材,持续3s.");return;
  }
  const int index = findRecord(id);
  if (index < 0) { server.send(404, "text/plain; charset=utf-8", "找不到该耗材");return; }
  if (!activateSpool(index, stableRaw, true)) { server.send(507, "text/plain; charset=utf-8", "保存耗材失败");return; }
  server.send(200, "text/plain; charset=utf-8", "OK");
}

void handleUpdate() {
  if (!requireSpoolMode()) return;
  const int index = requestedRecord();
  if (index < 0) return;
  const String color = server.arg("color"), materialText = server.arg("material");
  bool validColor = color.length() == 7 && color[0] == '#';
  for (uint8_t i = 1; validColor && i < 7; ++i) validColor = isxdigit(color[i]);
  if (!validColor || materialText.length() != 1 || materialText[0] < '0' || materialText[0] > '4') {
    server.send(400, "text/plain; charset=utf-8", "颜色或材料格式无效");return;
  }
  const auto previous = inventoryStore.records[index];
  auto &record = inventoryStore.records[index];
  float tare = previous.tare;
  if (server.hasArg("tare") && !server.arg("tare").isEmpty() &&
      (!parseNonnegativeGrams(server.arg("tare"), tare) || tare > record.total)) {
    server.send(400, "text/plain; charset=utf-8", "空盘重量必须是有效克数且不能超过记录总重量");return;
  }
  color.toCharArray(record.color, sizeof(record.color));
  record.material = materialText[0] - '0';
  record.tare = tare;
  if (!saveInventory()) { record = previous;server.send(507, "text/plain; charset=utf-8", "保存失败");return; }
  server.send(200, "text/plain; charset=utf-8", "OK");
}

void handleDelete() {
  if (!requireSpoolMode()) return;
  const int index = requestedRecord();
  if (index < 0) return;
  if (activeIndex == index && (spoolState == ACTIVE_SPOOL || unloadPending)) {
    server.send(409, "text/plain; charset=utf-8", "请先取下当前耗材并等待保存后再删除");return;
  }
  const InventoryStore previous = inventoryStore;
  const size_t moveCount = inventoryStore.count - index - 1;
  if (moveCount) memmove(&inventoryStore.records[index], &inventoryStore.records[index + 1], moveCount * sizeof(inventory::SpoolRecord));
  --inventoryStore.count;
  if (!saveInventory()) { inventoryStore = previous;server.send(507, "text/plain; charset=utf-8", "删除失败");return; }
  if (activeIndex > index) --activeIndex;
  server.send(200, "text/plain; charset=utf-8", "OK");
}

// Persist the chosen mode so reboot cannot resume inventory writes in ordinary mode.
void handleMode() {
  const String requested = server.arg("mode");
  if (requested != "spool" && requested != "scale") { server.send(400, "text/plain; charset=utf-8", "无效模式");return; }
  const RunMode next = requested == "spool" ? SPOOL_MODE : SCALE_MODE;
  if (next == runMode) { server.send(200, "text/plain; charset=utf-8", "OK");return; }
  if (runMode == SPOOL_MODE && !persistConsumedWeight()) {
    server.send(507, "text/plain; charset=utf-8", "保存上一耗材失败，模式未切换");return;
  }
  if (preferences.putUChar("mode", next) != 1) { server.send(507, "text/plain; charset=utf-8", "保存模式失败");return; }
  runMode = next;
  resetSpoolDetection();
  server.send(200, "text/plain; charset=utf-8", "OK");
}
