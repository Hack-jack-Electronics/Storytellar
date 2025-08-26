#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>

#define WIFI_SSID "reenanup_2.4G"
#define WIFI_PASSWORD "15772424"

TFT_eSPI tft = TFT_eSPI();

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}


// Function to download and save image
bool downloadImage(const char *url, const char *savePath) {
  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ HTTP GET failed: %d\n", httpCode);
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  File file = SPIFFS.open(savePath, FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to open file for writing");
    return false;
  }

  uint8_t buff[128] = { 0 };
  int len = http.getSize();
  int total = 0;

  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
      file.write(buff, c);
      total += c;
      if (len > 0) len -= c;
    }
    delay(1);
  }

  file.close();
  http.end();

  Serial.printf("✅ Download complete (%d bytes)\n", total);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ Connected");

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS Mount Failed");
    return;
  }

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  const char* imageUrl = "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_6f03d161/images/scene_1_grayscale.jpg";
  const char* savePath = "/image.jpg";

  Serial.printf("Downloading from: %s\n", imageUrl);
  if (downloadImage(imageUrl, savePath)) {
    TJpgDec.drawFsJpg(0, 0, savePath);
  } else {
    Serial.println("❌ Image download failed");
  }
}

void loop() {
  delay(1000);
}
