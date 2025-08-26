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

// Volume control pins and variables
const int VOLUME_UP_PIN = 32;
const int VOLUME_DOWN_PIN = 33;
const int MUTE_PIN = 34;

float currentVolume = 0.5;  // Start at 50% volume
float targetVolume = 0.5;   // Target volume for smooth transitions
const float VOLUME_STEP = 0.02;  // Smooth volume change step
const float MIN_VOLUME = 0.0;
const float MAX_VOLUME = 1.0;

// Button handling variables
unsigned long lastVolumeUpPress = 0;
unsigned long lastVolumeDownPress = 0;
unsigned long lastMutePress = 0;
const unsigned long BUTTON_DEBOUNCE = 50;
const unsigned long VOLUME_REPEAT_DELAY = 150;  // Faster repeat for held buttons

bool volumeUpPressed = false;
bool volumeDownPressed = false;
bool mutePressed = false;
bool isMuted = false;
float volumeBeforeMute = 0.5;

// Smooth volume transition variables
unsigned long lastVolumeUpdate = 0;
const unsigned long VOLUME_UPDATE_INTERVAL = 10;  // Update every 10ms for smooth transition

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup volume control pins
  pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
  pinMode(MUTE_PIN, INPUT_PULLUP);

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
  out->SetGain(currentVolume); // Set initial volume

  // Setup streaming MP3 from URL
  file = new AudioFileSourceICYStream(streamURL);
  
  // Create MP3 decoder
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);

  Serial.println("Volume Controls:");
  Serial.println("GPIO32 - Volume Up");
  Serial.println("GPIO33 - Volume Down");
  Serial.println("GPIO34 - Mute/Unmute");
  Serial.printf("Initial Volume: %.0f%%\n", currentVolume * 100);
}

void handleVolumeButtons() {
  unsigned long currentTime = millis();
  
  // Read button states (inverted because of INPUT_PULLUP)
  bool volUpState = !digitalRead(VOLUME_UP_PIN);
  bool volDownState = !digitalRead(VOLUME_DOWN_PIN);
  bool muteState = !digitalRead(MUTE_PIN);

  // Handle Volume Up button
  if (volUpState && !volumeUpPressed) {
    volumeUpPressed = true;
    lastVolumeUpPress = currentTime;
    if (!isMuted && targetVolume < MAX_VOLUME) {
      targetVolume = min(MAX_VOLUME, targetVolume + VOLUME_STEP * 3);  // Initial bigger step
      Serial.printf("Volume Up: %.0f%%\n", targetVolume * 100);
    }
  } else if (volUpState && volumeUpPressed && (currentTime - lastVolumeUpPress >= VOLUME_REPEAT_DELAY)) {
    lastVolumeUpPress = currentTime;
    if (!isMuted && targetVolume < MAX_VOLUME) {
      targetVolume = min(MAX_VOLUME, targetVolume + VOLUME_STEP * 2);  // Repeat steps
      Serial.printf("Volume Up: %.0f%%\n", targetVolume * 100);
    }
  } else if (!volUpState) {
    volumeUpPressed = false;
  }

  // Handle Volume Down button
  if (volDownState && !volumeDownPressed) {
    volumeDownPressed = true;
    lastVolumeDownPress = currentTime;
    if (!isMuted && targetVolume > MIN_VOLUME) {
      targetVolume = max(MIN_VOLUME, targetVolume - VOLUME_STEP * 3);  // Initial bigger step
      Serial.printf("Volume Down: %.0f%%\n", targetVolume * 100);
    }
  } else if (volDownState && volumeDownPressed && (currentTime - lastVolumeDownPress >= VOLUME_REPEAT_DELAY)) {
    lastVolumeDownPress = currentTime;
    if (!isMuted && targetVolume > MIN_VOLUME) {
      targetVolume = max(MIN_VOLUME, targetVolume - VOLUME_STEP * 2);  // Repeat steps
      Serial.printf("Volume Down: %.0f%%\n", targetVolume * 100);
    }
  } else if (!volDownState) {
    volumeDownPressed = false;
  }

  // Handle Mute button (toggle on press)
  if (muteState && !mutePressed && (currentTime - lastMutePress >= BUTTON_DEBOUNCE)) {
    mutePressed = true;
    lastMutePress = currentTime;
    
    if (isMuted) {
      // Unmute
      isMuted = false;
      targetVolume = volumeBeforeMute;
      Serial.printf("Unmuted - Volume: %.0f%%\n", targetVolume * 100);
    } else {
      // Mute
      isMuted = true;
      volumeBeforeMute = targetVolume;
      targetVolume = 0.0;
      Serial.println("Muted");
    }
  } else if (!muteState) {
    mutePressed = false;
  }
}

void updateVolume() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastVolumeUpdate >= VOLUME_UPDATE_INTERVAL) {
    lastVolumeUpdate = currentTime;
    
    // Smooth volume transition
    if (abs(currentVolume - targetVolume) > 0.001) {  // Only update if difference is significant
      if (currentVolume < targetVolume) {
        currentVolume = min(targetVolume, currentVolume + VOLUME_STEP);
      } else if (currentVolume > targetVolume) {
        currentVolume = max(targetVolume, currentVolume - VOLUME_STEP);
      }
      
      // Apply the volume change to the audio output
      out->SetGain(currentVolume);
    }
  }
}

void loop() {
  // Handle volume control buttons
  handleVolumeButtons();
  
  // Update volume smoothly
  updateVolume();
  
  // Handle audio streaming
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.println("MP3 stream ended. Restarting...");
      mp3->stop();
      delay(100);  // Reduced delay to minimize audio interruption
      delete file;
      file = new AudioFileSourceICYStream(streamURL);
      mp3->begin(file, out);
      // Restore volume after restart
      out->SetGain(currentVolume);
    }
  } else {
    Serial.println("MP3 not running. Trying to restart...");
    delay(100);  // Reduced delay
    delete file;
    file = new AudioFileSourceICYStream(streamURL);
    mp3->begin(file, out);
    // Restore volume after restart
    out->SetGain(currentVolume);
  }
}