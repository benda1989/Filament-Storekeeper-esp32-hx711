void servePage(const char *plain, size_t plainSize, const uint8_t *gzip, size_t gzipSize,
               const char *identityTag, const char *gzipTag) {
  const String encoding = server.header("Accept-Encoding");
  const bool compressed = webcache::acceptsGzip(encoding.c_str());
  const char *etag = compressed ? gzipTag : identityTag;
  server.sendHeader("Cache-Control", "private, no-cache");
  server.sendHeader("Vary", "Accept-Encoding");
  server.sendHeader("ETag", etag);
  const String requestedTag = server.header("If-None-Match");
  if (webcache::etagMatches(requestedTag.c_str(), etag)) {
    server.send(304, "text/html; charset=utf-8", "");
    return;
  }
  if (compressed) server.sendHeader("Content-Encoding", "gzip");
  const unsigned long started = millis();
  server.send_P(200, "text/html; charset=utf-8",
                compressed ? reinterpret_cast<const char *>(gzip) : plain,
                compressed ? gzipSize : plainSize);
  Serial.printf("HTTP page: %s, %u bytes, %lu ms\n", compressed ? "gzip" : "identity",
                static_cast<unsigned>(compressed ? gzipSize : plainSize), millis() - started);
}

void handleScalePage() {
  servePage(PAGE, sizeof(PAGE) - 1, PAGE_GZIP, sizeof(PAGE_GZIP), PAGE_IDENTITY_ETAG, PAGE_GZIP_ETAG);
}
void handleSettingsPage() {
  servePage(SETTINGS_PAGE, sizeof(SETTINGS_PAGE) - 1, SETTINGS_PAGE_GZIP, sizeof(SETTINGS_PAGE_GZIP),
            SETTINGS_PAGE_IDENTITY_ETAG, SETTINGS_PAGE_GZIP_ETAG);
}
