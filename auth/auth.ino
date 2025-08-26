#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/md.h"
#include <time.h>

const char* WIFI_SSID = "reenanup_2.4G";
const char* WIFI_PASS = "15772424";
const char* SERVER_URL = "http://34.42.234.149:8000";
#define DEVICE_ID      "a1b2c3d4-e5f6-7890-abcd-ef1234567891"
#define DEVICE_SECRET  "esp32-secret-key-2026"

String current_jwt_token = "";
unsigned long jwt_expiry = 0;

String calculateHMAC(const String& deviceSecret, const String& method, const String& path, const String& body, long timestamp) {
  String msg = method + "|" + path + "|" + body + "|" + String(timestamp);
  uint8_t hmac[32];
  mbedtls_md_context_t ctx; mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)deviceSecret.c_str(), strlen(deviceSecret.c_str()));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)msg.c_str(), msg.length());
  mbedtls_md_hmac_finish(&ctx, hmac); mbedtls_md_free(&ctx);

  char signature[65];
  for (int i=0; i<32; ++i) sprintf(&signature[i*2], "%02x", (unsigned int)hmac[i]);
  signature[64]=0;
  return String(signature);
}

void setupTime() {
  setenv("TZ", "UTC0", 1); tzset();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  Serial.print("[NTP] Sync...");
  time_t now=0; int retry=0;
  while(now<1700000000 && retry<30) { delay(1000); now=time(nullptr); Serial.print("."); retry++; }
  Serial.println();
  if(now<1700000000) { Serial.println("[NTP] FAIL."); ESP.restart(); }
  Serial.printf("[NTP] OK, Epoch: %ld\n", now);
}

void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s ... ", WIFI_SSID);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) { delay(500); Serial.print("."); }
  if (WiFi.status() != WL_CONNECTED) { Serial.println(" FAIL!"); ESP.restart(); }
  Serial.print(" Connected. IP: "); Serial.println(WiFi.localIP());
}

void testDeviceAuthorization() {
  Serial.println("[Auth] Requesting JWT session token...");

  HTTPClient http;
  String path = "/iot/devices/session";
  String url = String(SERVER_URL) + path;
  http.begin(url);

  DynamicJsonDocument doc(256);
  doc["device_name"] = "Living Room Storyteller";
  String body; serializeJson(doc, body);

  long timestamp = time(nullptr);
  String sig = calculateHMAC(DEVICE_SECRET, "POST", path, body, timestamp);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Id", DEVICE_ID);
  http.addHeader("X-Timestamp", String(timestamp));
  http.addHeader("X-Signature", sig);

  int resp = http.POST(body);
  Serial.print("[Auth] HTTP code: "); Serial.println(resp);

  if (resp > 0) {
    String payload = http.getString();
    Serial.println("[Auth] Response JSON:");
    Serial.println(payload);

    // -- Parse "device_jwt", "expires_in", "device_id" --
    StaticJsonDocument<1024> resDoc;
    DeserializationError err = deserializeJson(resDoc, payload);
    if (!err && resDoc.containsKey("device_jwt")) {
      current_jwt_token = (const char*)resDoc["device_jwt"];
      jwt_expiry = millis() + (resDoc["expires_in"].as<unsigned long>() * 1000);
      String got_device_id = resDoc["device_id"].as<String>();
      Serial.println("[Auth] JWT:");
      Serial.println(current_jwt_token);
      Serial.print("[Auth] Expires in (s): "); Serial.println(resDoc["expires_in"].as<long>());
      Serial.print("[Auth] Device ID: "); Serial.println(got_device_id);
    } else {
      Serial.println("[Auth] JWT NOT FOUND in response.");
    }
    http.end();
  } else {
    Serial.println("[Auth] HTTP Error!"); http.end();
  }
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
  setupTime();
  testDeviceAuthorization();
}

void loop() {
  delay(10000);
}
