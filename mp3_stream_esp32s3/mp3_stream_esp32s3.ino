#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// WiFi credentials
const char *ssid = "reenanup_2.4G";
const char *password = "15772424";

// Audio stream URL
const char *streamURL = "http://www.archive.org/download/MLKDream/MLKDream_64kb.mp3";  // your custom stream

// Audio objects
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Initialize I2S for ESP32-S3
  out = new AudioOutputI2S();
  out->SetPinout(10,9,11);  // BCLK, LRC (WS), DIN (Data Out) - Choose free GPIOs for ESP32-S3
  out->SetGain(0.8);          // Volume control (0.0 to 1.0)

  // Start streaming MP3
  file = new AudioFileSourceICYStream(streamURL);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
}

void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("MP3 stream ended. Restarting...");
      mp3->stop();
      delay(500);
      delete file;
      file = new AudioFileSourceICYStream(streamURL);
      mp3->begin(file, out);
    }
  } else {
    Serial.println("MP3 not running. Trying to restart...");
    delay(500);
    delete file;
    file = new AudioFileSourceICYStream(streamURL);
    mp3->begin(file, out);
  }
}
