void handleFirmwareUpload() {
  HTTPUpload &upload = server.upload();
  if (otaUploading && millis() - otaStartedAt >= 120000) {
    Update.abort();
    otaUploading = false;
    otaError = "上传时间过长，当前固件未切换，请改善连接后重试";
    server.client().stop();
    Serial.println("OTA upload exceeded time budget");
    return;
  }
  if (upload.status == UPLOAD_FILE_START) {
    // The core sets its read timeout before parsing; override it at file start.
    server.client().setTimeout(2000);
    otaSucceeded = false;
    otaError = "更新暂不可用";
    if (otaUploading || otaRebootPending) return;
    if (wifiChangePending) { otaError = "正在切换 Wi-Fi，请稍后再更新固件"; return; }
    if (upload.name != "firmware") {
      otaError = "无效的固件字段";
      return;
    }
    String filename = upload.filename;
    filename.toLowerCase();
    if (!filename.endsWith(".bin")) {
      otaError = "请选择 .bin 固件文件";
      return;
    }
    if (esp_ota_get_next_update_partition(nullptr) == nullptr) {
      otaError = "当前分区不支持在线更新";
      return;
    }
    if (!persistConsumedWeight()) {
      otaError = "保存当前耗材失败，更新已取消";
      return;
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      otaError = Update.errorString();
      Serial.printf("OTA begin failed: %s\n", otaError.c_str());
      return;
    }
    otaUploading = true;
    otaStartedAt = millis();
    otaError = "";
    Serial.printf("OTA upload started: %s\n", filename.c_str());
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!otaUploading) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaError = Update.errorString();
      Update.abort();
      otaUploading = false;
      Serial.printf("OTA write failed: %s\n", otaError.c_str());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!otaUploading) return;
    if (upload.totalSize == 0 || !Update.end(true)) {
      otaError = upload.totalSize == 0 ? "固件文件为空" : Update.errorString();
      Update.abort();
      Serial.printf("OTA finalize failed: %s\n", otaError.c_str());
    } else {
      otaSucceeded = true;
      Serial.printf("OTA complete: %lu bytes\n", static_cast<unsigned long>(upload.totalSize));
    }
    otaUploading = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (otaUploading) Update.abort();
    otaUploading = false;
    if (otaError.isEmpty()) otaError = "上传中断，当前固件未切换";
    Serial.println("OTA upload aborted");
  }
}

void handleFirmwareResult() {
  if (otaUploading) {
    Update.abort();
    otaUploading = false;
    otaError = "固件上传未完成";
  }
  if (!otaSucceeded) {
    server.send(400, "text/plain; charset=utf-8", otaError.isEmpty() ? "固件更新失败" : otaError);
    return;
  }
  otaSucceeded = false;
  server.send(200, "text/plain; charset=utf-8", "更新成功，设备即将重启");
  otaRebootAt = millis();
  otaRebootPending = true;
}
