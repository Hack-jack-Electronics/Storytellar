#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// WiFi credentials
const char *ssid = "reenanup_2.4G";
const char *password = "15772424";

// Internet radio stream
const char *streamURL = "http://www.archive.org/download/MLKDream/MLKDream_64kb.mp3";

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
  Serial.println("\nWiFi connected.");

  // Setup I2S audio output
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27);  // BCLK, LRC, DIN
  out->SetGain(1.0);           // Volume from 0.0 to 1.0

  // Setup streaming MP3 from URL
  file = new AudioFileSourceICYStream(streamURL);
  // file->SetBufferSize(2048);  // Only if your version supports this method

  // Create MP3 decoder
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
