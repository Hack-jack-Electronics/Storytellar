#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// WiFi Credentials
#define WIFI_SSID "reenanup_2.4G"
#define WIFI_PASSWORD "15772424"

// TFT Setup
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI_Button dummy;

// Scene image/audio pairs
const char* imageUrls[] = {
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_6f03d161/images/scene_1_grayscale.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_6f03d161/images/scene_2_grayscale.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_6f03d161/images/scene_3_grayscale.jpg"
};

const char* audioUrls[] = {
  "http://www.archive.org/download/MLKDream/MLKDream_64kb.mp3",
  "http://www.archive.org/download/testmp3testfile/mpthreetest.mp3",
  "http://www.archive.org/download/yourmp3example/yourmp3example.mp3"
};

int currentScene = 0;
int totalScenes = sizeof(imageUrls) / sizeof(imageUrls[0]);

bool isTouched = false;
bool isPlaying = true;

// Audio components
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceICYStream *file = nullptr;
AudioOutputI2S *out = nullptr;

// JPEG callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Download image
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

  uint8_t buff[128];
  int len = http.getSize();
  int total = 0;

  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(buff, (size > sizeof(buff)) ? sizeof(buff) : size);
      file.write(buff, c);
      total += c;
      if (len > 0) len -= c;
    }
    delay(1);  // 👈 allows task switching
    yield();   // 👈 important to avoid watchdog
  }

  file.close();
  http.end();
  Serial.printf("✅ Image downloaded (%d bytes)\n", total);
  return true;
}

// Display image
void showImage(const char* url) {
  const char* savePath = "/image.jpg";
  tft.fillScreen(TFT_BLACK);
  if (downloadImage(url, savePath)) {
    TJpgDec.drawFsJpg(0, 0, savePath);
  } else {
    Serial.println("❌ Failed to show image");
  }
}

// Start or restart audio
void startAudio(const char* url) {
  if (mp3 && mp3->isRunning()) mp3->stop();
  if (file) delete file;
  file = new AudioFileSourceICYStream(url);
  mp3->begin(file, out);
  isPlaying = true;
}

// Toggle play/pause
void togglePlayPause() {
  if (!mp3) return;
  if (isPlaying) {
    mp3->stop();
    isPlaying = false;
    Serial.println("⏸️ Paused");
  } else {
    file = new AudioFileSourceICYStream(audioUrls[currentScene]);
    mp3->begin(file, out);
    isPlaying = true;
    Serial.println("▶️ Resumed");
  }
}

// Play/pause button
void drawPlayPauseButton(bool playing) {
  int x = 250, y = 200, w = 60, h = 30;
  tft.fillRect(x, y, w, h, TFT_WHITE);
  tft.drawRect(x, y, w, h, TFT_BLACK);
  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(x + 5, y + 7);
  tft.print(playing ? "Pause" : "Play");
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    yield();  // 👈 allow task switching during connection
  }
  Serial.println("\n✅ WiFi connected");

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
    return;
  }

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27);  // BCLK, LRC, DIN
  out->SetGain(0.8);           // 🔊 Volume

  mp3 = new AudioGeneratorMP3();

  showImage(imageUrls[currentScene]);
  startAudio(audioUrls[currentScene]);
  drawPlayPauseButton(true);
}

void loop() {
  // 🧠 Keep audio streaming
  if (mp3 && isPlaying && !mp3->loop()) {
    mp3->stop();
    isPlaying = false;
    Serial.println("🔁 Audio ended");
  }

  // 👆 Touch handling
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (!isTouched) {
      isTouched = true;

      if (x >= 250 && x <= 310 && y >= 200 && y <= 230) {
        togglePlayPause();
        drawPlayPauseButton(isPlaying);
      } else {
        currentScene = (currentScene + 1) % totalScenes;
        showImage(imageUrls[currentScene]);
        startAudio(audioUrls[currentScene]);
        drawPlayPauseButton(true);
      }
    }
  } else {
    isTouched = false;
  }

  delay(1);  // 👈 Prevent watchdog reset
  yield();   // 👈 Let ESP32 do background tasks
}
