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
AudioOutputI2S *out;

// ----------- File List -------------------
String fileList[50];  // max 50 files
int fileCount = 0;
int currentTrack = 0;  // Currently playing track

// ----------- Volume Control --------------
float currentVolume = 0.5f;
float targetVolume  = 0.5f;
const float VOLUME_STEP = 0.05f;
const float MIN_VOLUME = 0.0f;
const float MAX_VOLUME = 1.0f;
unsigned long lastVolumeUpdate = 0;
const unsigned long VOLUME_UPDATE_INTERVAL = 20;

// ----------- Setup ---------------
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

  // Setup I2S
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27); // BCLK=26, LRC=25, DIN=27
  out->SetGain(currentVolume);

  // Setup buttons
  pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
  pinMode(NEXT_TRACK_PIN, INPUT_PULLUP);
  pinMode(PREV_TRACK_PIN, INPUT_PULLUP);

  Serial.println("\nEnter the file number to play or use buttons:");
  Serial.println("Commands: next, prev, vol, up, down, list");
}

// ----------- Loop ---------------
void loop() {
  // Smooth volume update
  updateVolume();

  // Handle serial commands
  handleSerialCommands();

  // Handle buttons
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

// -------- Play WAV from SD --------
void playWAV(String filename) {
  if (wav) { if (wav->isRunning()) wav->stop(); delete wav; wav = nullptr; }
  if (file) { delete file; file = nullptr; }

  Serial.println("\n▶️ Playing: " + filename);
  file = new AudioFileSourceSD(filename.c_str());
  wav = new AudioGeneratorWAV();
  if (!wav->begin(file, out)) {
    Serial.println("❌ WAV playback failed");
    delete file; file = nullptr;
    delete wav; wav = nullptr;
  }
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

// -------- Volume Update (Smooth) --------
void updateVolume() {
  if (millis() - lastVolumeUpdate >= VOLUME_UPDATE_INTERVAL) {
    lastVolumeUpdate = millis();
    if (abs(currentVolume - targetVolume) > 0.01f) {
      if (currentVolume < targetVolume) currentVolume += VOLUME_STEP;
      else if (currentVolume > targetVolume) currentVolume -= VOLUME_STEP;
      out->SetGain(currentVolume);
    }
  }
}

// -------- Handle Buttons --------
void handleButtons() {
  if (!digitalRead(VOLUME_UP_PIN)) {
    if (targetVolume < MAX_VOLUME) targetVolume += 0.1f;
    Serial.printf("[VOLUME] Up: %.0f%%\n", targetVolume * 100);
    delay(200);
  }
  if (!digitalRead(VOLUME_DOWN_PIN)) {
    if (targetVolume > MIN_VOLUME) targetVolume -= 0.1f;
    Serial.printf("[VOLUME] Down: %.0f%%\n", targetVolume * 100);
    delay(200);
  }
  if (!digitalRead(NEXT_TRACK_PIN)) {
    currentTrack = (currentTrack + 1) % fileCount;
    playWAV(fileList[currentTrack]);
    delay(300);
  }
  if (!digitalRead(PREV_TRACK_PIN)) {
    currentTrack = (currentTrack - 1 + fileCount) % fileCount;
    playWAV(fileList[currentTrack]);
    delay(300);
  }
}

// -------- Handle Serial Commands --------
void handleSerialCommands() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("next")) {
      currentTrack = (currentTrack + 1) % fileCount;
      playWAV(fileList[currentTrack]);
    }
    else if (input.equalsIgnoreCase("prev")) {
      currentTrack = (currentTrack - 1 + fileCount) % fileCount;
      playWAV(fileList[currentTrack]);
    }
    else if (input.equalsIgnoreCase("up")) {
      targetVolume = min(MAX_VOLUME, targetVolume + 0.1f);
      Serial.printf("[CMD] Volume up: %.0f%%\n", targetVolume * 100);
    }
    else if (input.equalsIgnoreCase("down")) {
      targetVolume = max(MIN_VOLUME, targetVolume - 0.1f);
      Serial.printf("[CMD] Volume down: %.0f%%\n", targetVolume * 100);
    }
    else if (input.equalsIgnoreCase("list")) {
      Serial.println("[TRACKS]");
      for (int i = 0; i < fileCount; i++) {
        Serial.printf("%d: %s %s\n", i, fileList[i].c_str(), (i == currentTrack ? "<-- Playing" : ""));
      }
    }
    else if (input.length() > 0 && isDigit(input.charAt(0))) {
      int track = input.toInt();
      if (track >= 0 && track < fileCount) {
        currentTrack = track;
        playWAV(fileList[currentTrack]);
      } else {
        Serial.printf("[ERROR] Invalid track number (0-%d)\n", fileCount - 1);
      }
    }
  }
}
