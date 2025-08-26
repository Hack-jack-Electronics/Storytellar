#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <time.h>

Preferences preferences;

// SoftAP config
const char* ap_ssid = "ESP32_Setup";
const char* ap_password = "esp32pass";
const char* SERVER_URL = "http://34.42.234.149:8000";

#define DEVICE_ID      "9d9e6c9b956440a0a87c07cdc36c6500"
#define DEVICE_SECRET  "test-device-secret-123"

WebServer server(80);
bool provisioned = false;

// ---------- HMAC-SHA256 signature for IoT Device Claim ----------
String calculateHMAC(const String& deviceSecret,
                     const String& method,
                     const String& path, 
                     const String& requestBody,
                     long timestamp) {
  // Server expects: METHOD|PATH|BODY|TIMESTAMP
  String message = method + "|" + path + "|" + requestBody + "|" + String(timestamp);

  uint8_t hmac[32];

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  const size_t key_len = strlen(deviceSecret.c_str());

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)deviceSecret.c_str(), key_len);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  char signature[65];
  for (int i = 0; i < 32; ++i) sprintf(&signature[i * 2], "%02x", (unsigned int)hmac[i]);
  signature[64] = 0;  // Null-terminate

  return String(signature);
}

// ---------- Provisioning Server ----------
void handleRoot() {
  server.send(200, "text/html",
    "<form method='POST' action='/provision'>"
    "SSID:<input name='ssid'><br>"
    "PWD:<input name='password' type='password'><br>"
    "Token:<input name='claim_token'><br>"
    "<input type='submit' value='Save & Reboot'>"
    "</form>"
  );
}

void handleProvision() {
  String ssid, password, claim_token;
  String contentType = server.header("Content-Type");

  if(contentType.indexOf("application/json") >= 0) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
      Serial.print("JSON Parse failed: "); Serial.println(error.c_str());
      server.send(400, "application/json", "{\"error\":\"Malformed JSON\"}");
      return;
    }
    ssid = doc["ssid"] | "";
    password = doc["password"] | "";
    claim_token = doc["claim_token"] | "";

    Serial.println("Received JSON:");
    serializeJsonPretty(doc, Serial); Serial.println();

    if (ssid == "" || password == "" || claim_token == "") {
      server.send(400, "application/json", "{\"error\":\"Missing fields\"}");
      return;
    }
  } else {
    ssid = server.arg("ssid");
    password = server.arg("password");
    claim_token = server.arg("claim_token");

    StaticJsonDocument<256> doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["claim_token"] = claim_token;
    Serial.println("Received Form:");
    serializeJsonPretty(doc, Serial); Serial.println();

    if (ssid == "" || password == "" || claim_token == "") {
      server.send(400, "text/plain", "Missing form fields");
      return;
    }
  }

  preferences.begin("provision", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("claim_token", claim_token);
  preferences.end();

  server.send(200, "application/json", "{\"status\":\"ok\", \"message\":\"Provisioned. Rebooting.\"}");
  delay(1500);
  ESP.restart();
}

// ---------- NTP / RTC Setup (for correct current time) ----------
void setupTime() {
  configTime(0, 0, "pool.ntp.org");
  Serial.print("[*] Waiting for NTP time sync...");
  time_t now = 0;
  int retries = 0;
  while(now < 8 * 3600 * 2 && retries < 15) {
    delay(1000);
    now = time(nullptr);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  if(now < 8 * 3600 * 2) {
    Serial.println("[-] Failed to sync NTP! Claim/Signature may use old time.");
  } else {
    Serial.print("[+] Time acquired: "); Serial.println(now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("%04d-%02d-%02dT%02d:%02d:%02dZ\n",
      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
}

// ---------- WiFi Connect with Retry ----------
bool connectToWiFi(const String& ssid, const String& password, int maxAttempts = 10) {
  Serial.println("[*] Attempting WiFi connection...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int tryCount = 0;
  while (WiFi.status() != WL_CONNECTED && tryCount < maxAttempts) {
    delay(1000); Serial.print(".");
    tryCount++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[+] WiFi connected, IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("[-] WiFi connect failed.");
  return false;
}

// ---------- Device Claim with new HMAC Auth format ----------
bool claimDeviceOnCloud(const String& claim_token) {
  Serial.println("[*] Claiming device on cloud...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[-] Not connected to WiFi, skipping claim");
    return false;
  }

  HTTPClient http;
  String claimPath = "/iot/devices/claim";
  String url = String(SERVER_URL) + claimPath;
  http.begin(url);

  // Build JSON body
  DynamicJsonDocument doc(512);
  doc["claim_token"] = claim_token;
  doc["device_name"] = "Living Room Storyteller";
  String requestBody;
  serializeJson(doc, requestBody);

  // Use current, accurate time
  long timestamp = (long)time(nullptr);

  String signature = calculateHMAC(
    DEVICE_SECRET,
    "POST",
    claimPath,
    requestBody,
    timestamp
  );

  // Debug output!
  Serial.println("=== HMAC Debug ===");
  Serial.println("Device Secret: " + String(DEVICE_SECRET));
  Serial.println("Method: POST");
  Serial.println("Path: " + claimPath);
  Serial.println("Body: " + requestBody);
  Serial.println("Timestamp: " + String(timestamp));
  Serial.println("HMAC Message: POST|" + claimPath + "|" + requestBody + "|" + String(timestamp));
  Serial.println("Signature: " + signature);
  Serial.println("==================");

  // Set HTTP headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", DEVICE_ID);
  http.addHeader("X-Timestamp", String(timestamp));
  http.addHeader("X-Signature", signature);

  int resp = http.POST(requestBody);
  Serial.print("<- Server response code: "); Serial.println(resp);

  if (resp > 0) {
    String payload = http.getString();
    Serial.println("<- Server response body:");
    Serial.println(payload);
    http.end();
    return (resp == 200);
  } else {
    Serial.println("HTTP Error: " + String(resp));
    http.end();
    return false;
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  // Check if already provisioned
  preferences.begin("provision", true);
  String ssid = preferences.getString("ssid", "");
  String pwd = preferences.getString("password", "");
  String claim_token = preferences.getString("claim_token", "");
  preferences.end();

  if (ssid.length() > 0 && pwd.length() > 0 && claim_token.length() > 0) {
    provisioned = true;
  }

  if (!provisioned) {
    Serial.println("[*] Starting provisioning AP/server...");
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("Connect to AP: "); Serial.println(ap_ssid);
    Serial.print("SoftAP IP: "); Serial.println(WiFi.softAPIP());
    server.on("/", HTTP_GET, handleRoot);
    server.on("/provision", HTTP_POST, handleProvision);
    server.begin();
    Serial.println("[*] HTTP server started");
    return;
  } else {
    Serial.println("[*] Already provisioned, proceeding with WiFi/cloud connect...");
  }

  if (connectToWiFi(ssid, pwd)) {
    setupTime();
    claimDeviceOnCloud(claim_token);
  } else {
    Serial.println("[-] WiFi failed. Restarting for fallback...");
    delay(10000);
    ESP.restart();
  }
}

void loop() {
  if (!provisioned) {   
    server.handleClient();
    delay(10);
  }
  // Main logic here for device after claiming
}
