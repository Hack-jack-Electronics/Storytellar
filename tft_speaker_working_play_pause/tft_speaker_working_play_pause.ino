#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// ---------------- WiFi ------------------
const char *ssid = "reenanup_2.4G";
const char *password = "15772424";

// ----------- Audio Stream ---------------
const char *streamURL = "http://www.archive.org/download/MLKDream/MLKDream_64kb.mp3";
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;
bool isPlaying = true;

// ----------- TFT + Touch ---------------
#define TOUCH_CS_PIN  21
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS_PIN);  // IRQ optional
TFT_eSPI_Button toggleBtn;

// ----------- Images ---------------------
const uint16_t image1[160 * 160] PROGMEM = { /* Your PLAY image data */ };
const uint16_t image2[160 * 160] PROGMEM = { /* Your PAUSE image data */ };

// ----------- Helper: Draw Image ----------
void drawRGB565Image(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data) {
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  uint16_t buf[32];
  int pos = 0;
  while (pos < w * h) {
    int chunkSize = (w * h - pos >= 32) ? 32 : w * h - pos;
    for (int i = 0; i < chunkSize; i++) {
      buf[i] = pgm_read_word(&data[pos + i]);
    }
    tft.pushColors(buf, chunkSize, true);
    pos += chunkSize;
  }
  tft.endWrite();
}

// ----------- Draw Button -----------------
void drawButton() {
  char label[10];
  sprintf(label, isPlaying ? "Pause" : "Play");
  toggleBtn.initButton(&tft, 160, 220, 120, 40, TFT_WHITE, TFT_BLUE, TFT_WHITE, label, 2);
  toggleBtn.drawButton();
}

// ----------- Draw Image Based on State ---
void drawCurrentImage() {
  tft.fillScreen(TFT_BLACK);
  if (isPlaying)
    drawRGB565Image(0, 0, 160, 160, image1); // Image when playing
  else
    drawRGB565Image(0, 0, 160, 160, image2); // Image when paused
  drawButton();
}

// ----------- Setup -----------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  // I2S
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27);
  out->SetGain(1.0);

  // MP3 stream
  file = new AudioFileSourceICYStream(streamURL);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);

  // TFT + Touch
  tft.begin();
  tft.setRotation(1);
  ts.begin();
  ts.setRotation(1);

  drawCurrentImage();
}

// ----------- Loop ------------------------
void loop() {
  // Touch handling
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    int x = map(p.x, 300, 3800, 0, tft.width());
    int y = map(p.y, 3600, 300, 0, tft.height());

    if (toggleBtn.contains(x, y)) {
      isPlaying = !isPlaying;

      if (!isPlaying) {
        mp3->stop(); // Pause
      } else {
        delete file;
        file = new AudioFileSourceICYStream(streamURL);
        mp3->begin(file, out); // Resume
      }

      drawCurrentImage(); // Update screen
      delay(300); // debounce
    }
  }

  // Continue playback if playing
  if (isPlaying && mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("Stream ended. Restarting...");
      mp3->stop();
      delay(500);
      delete file;
      file = new AudioFileSourceICYStream(streamURL);
      mp3->begin(file, out);
    }
  }
}
