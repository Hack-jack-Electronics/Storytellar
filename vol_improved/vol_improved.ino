#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

// ----------- WiFi Credentials ------------
const char* ssid     = "reenanup_2.4G";
const char* password = "15772424";

// ----------- Firebase WAV URL ------------
const char* firebaseURL = "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_227a98b6/audio/scene_1.wav";

// ----------- SD Card Config --------------
#define SD_CS 5  // Adjust according to wiring

// ----------- Control Pins ----------------
const int VOLUME_UP_PIN   = 32;
const int VOLUME_DOWN_PIN = 33;
const int NEXT_TRACK_PIN  = 12;
const int PREV_TRACK_PIN  = 13;

// ----------- Audio Objects ---------------
AudioGeneratorWAV *wav = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputI2S *out = nullptr;

// ----------- File List -------------------
String fileList[50];
int fileCount = 0;
int currentTrack = 0;

// ----------- Volume Control --------------
float currentVolume = 0.5f;
const float VOLUME_STEP = 0.1f;
const float MIN_VOLUME = 0.0f;
const float MAX_VOLUME = 1.0f;

void setup() {
  Serial.begin(115200);

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  // Init SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card Mount Failed");
    while (true);
  }
  Serial.println("✅ SD card initialized");

  // Download default file if missing
  String filename = "/scene_1.wav";
  if (!SD.exists(filename)) {
    Serial.println("⬇️ File not found, downloading...");
    downloadAudio(firebaseURL, filename.c_str());
  }

  // List files
  Serial.println("\n📂 WAV files on SD card:");
  listFiles(SD, "/", 0);

  // Setup buttons
  pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
  pinMode(NEXT_TRACK_PIN, INPUT_PULLUP);
  pinMode(PREV_TRACK_PIN, INPUT_PULLUP);

  Serial.println("\nEnter the file number to play or use buttons:");
  Serial.println("Commands: next, prev, up, down, list");

  // Setup I2S and play first file
  if (fileCount > 0) {
    startPlayback(currentTrack);
  }
}

void loop() {
  handleSerialCommands();
  handleButtons();

  // Keep playback alive
  if (wav && wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      Serial.println("\n✅ Playback finished");
    }
  }
}

// -------- Download WAV from Firebase --------
void downloadAudio(const char* url, const char* path) {
  Serial.printf("Downloading: %s\n", url);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
      Serial.println("❌ Failed to open file on SD for writing");
      return;
    }
    uint8_t buffer[512];
    while (http.connected()) {
      size_t sizeAvailable = stream->available();
      if (sizeAvailable) {
        int c = stream->readBytes(buffer, min(sizeAvailable, sizeof(buffer)));
        file.write(buffer, c);
      }
      delay(1);
    }
    file.close();
    Serial.println("✅ Download complete");
  } else {
    Serial.printf("❌ HTTP request failed, code: %d\n", httpCode);
  }
  http.end();
}

// -------- List all WAV files --------
void listFiles(fs::FS &fs, const char * dirname, uint8_t levels) {
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) return;
  fileCount = 0;
  File file = root.openNextFile();
  while (file && fileCount < 50) {
    if (!file.isDirectory()) {
      String fname = file.name();
      if (fname.endsWith(".wav") || fname.endsWith(".WAV")) {
        if (!fname.startsWith("/")) fname = "/" + fname;
        Serial.printf("[%d] %s\n", fileCount, fname.c_str());
        fileList[fileCount++] = fname;
      }
    }
    file = root.openNextFile();
  }
}

// -------- (Re)start playback from file index --------
void startPlayback(int idx) {
  // Teardown previous objects
  if (wav) { if (wav->isRunning()) wav->stop(); delete wav; wav = nullptr; }
  if (file) { delete file; file = nullptr; }
  if (out) { delete out; out = nullptr; }
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27); // Replace with your actual I2S pins/wiring!
  out->SetGain(currentVolume);

  Serial.printf("\n▶️ Playing: %s at %.0f%% volume\n", fileList[idx].c_str(), currentVolume * 100);

  file = new AudioFileSourceSD(fileList[idx].c_str());
  wav = new AudioGeneratorWAV();
  if (!wav->begin(file, out)) {
    Serial.println("❌ WAV playback failed");
    delete file; file = nullptr;
    delete wav; wav = nullptr;
  }
}

// -------- Change volume and restart playback --------
void changeVolumeAndRestart(float newVolume) {
  currentVolume = constrain(newVolume, MIN_VOLUME, MAX_VOLUME);
  startPlayback(currentTrack); // restarts with new gain from start of file
}

// -------- Handle Buttons --------
void handleButtons() {
  static unsigned long lastBtnTime = 0;
  if (millis() - lastBtnTime < 200) return; // debounce

  if (!digitalRead(VOLUME_UP_PIN)) {
    if (currentVolume < MAX_VOLUME) {
      changeVolumeAndRestart(currentVolume + VOLUME_STEP);
      Serial.printf("[VOLUME] Up: %.0f%%\n", currentVolume * 100);
    }
    lastBtnTime = millis();
  }
  if (!digitalRead(VOLUME_DOWN_PIN)) {
    if (currentVolume > MIN_VOLUME) {
      changeVolumeAndRestart(currentVolume - VOLUME_STEP);
      Serial.printf("[VOLUME] Down: %.0f%%\n", currentVolume * 100);
    }
    lastBtnTime = millis();
  }
  if (!digitalRead(NEXT_TRACK_PIN)) {
    currentTrack = (currentTrack + 1) % fileCount;
    startPlayback(currentTrack);
    lastBtnTime = millis();
  }
  if (!digitalRead(PREV_TRACK_PIN)) {
    currentTrack = (currentTrack - 1 + fileCount) % fileCount;
    startPlayback(currentTrack);
    lastBtnTime = millis();
  }
}

// -------- Handle Serial Commands --------
void handleSerialCommands() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("next")) {
      currentTrack = (currentTrack + 1) % fileCount;
      startPlayback(currentTrack);
    } else if (input.equalsIgnoreCase("prev")) {
      currentTrack = (currentTrack - 1 + fileCount) % fileCount;
      startPlayback(currentTrack);
    } else if (input.equalsIgnoreCase("up")) {
      if (currentVolume < MAX_VOLUME) {
        changeVolumeAndRestart(currentVolume + VOLUME_STEP);
        Serial.printf("[CMD] Volume up: %.0f%%\n", currentVolume * 100);
      }
    } else if (input.equalsIgnoreCase("down")) {
      if (currentVolume > MIN_VOLUME) {
        changeVolumeAndRestart(currentVolume - VOLUME_STEP);
        Serial.printf("[CMD] Volume down: %.0f%%\n", currentVolume * 100);
      }
    } else if (input.equalsIgnoreCase("list")) {
      Serial.println("[TRACKS]");
      for (int i = 0; i < fileCount; i++) {
        Serial.printf("%d: %s %s\n", i, fileList[i].c_str(), (i == currentTrack ? "<-- Playing" : ""));
      }
    } else if (input.length() > 0 && isDigit(input.charAt(0))) {
      int track = input.toInt();
      if (track >= 0 && track < fileCount) {
        currentTrack = track;
        startPlayback(currentTrack);
      } else {
        Serial.printf("[ERROR] Invalid track number (0-%d)\n", fileCount - 1);
      }
    }
  }
}
