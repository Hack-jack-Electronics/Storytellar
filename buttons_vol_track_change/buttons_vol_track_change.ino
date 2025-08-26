#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// WiFi credentials
const char *ssid = "reenanup_2.4G";
const char *password = "15772424";

// Multiple audio stream URLs
const char *streamURLs[] = {
    "http://www.archive.org/download/MLKDream/MLKDream_64kb.mp3",
    "http://192.168.1.9:9000/mediafiles/scene_2.mp3",
    "http://192.168.1.9:9000/mediafiles/scene_3.mp3",
    "http://192.168.1.9:9000/mediafiles/scene_4.mp3",
    "http://192.168.1.9:9000/mediafiles/scene_5.mp3",
    "http://192.168.1.9:9000/mediafiles/scene_6.mp3"
};

const int TOTAL_STREAMS = sizeof(streamURLs) / sizeof(streamURLs[0]);

// Audio objects
AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioOutputI2S *out;

// Track control variables
int currentTrack = 0;  // Start with first track
bool trackChangeRequested = false;
int requestedTrack = -1;

// Control pins
const int VOLUME_UP_PIN = 32;      // Volume up button
const int VOLUME_DOWN_PIN = 33;    // Volume down button
const int NEXT_TRACK_PIN = 18;     // Forward/Next track button
const int PREV_TRACK_PIN = 19;     // Backward/Previous track button

// Volume control variables
float currentVolume = 0.5f;  // Start at 50% volume
float targetVolume = 0.5f;   // Target volume for smooth transitions
const float VOLUME_STEP = 0.02f;  // Smooth volume change step
const float MIN_VOLUME = 0.0f;
const float MAX_VOLUME = 1.0f;

// Button handling variables
unsigned long lastVolumeUpPress = 0;
unsigned long lastVolumeDownPress = 0;
unsigned long lastNextTrackPress = 0;
unsigned long lastPrevTrackPress = 0;
const unsigned long BUTTON_DEBOUNCE = 50;
const unsigned long VOLUME_REPEAT_DELAY = 150;
const unsigned long TRACK_DEBOUNCE = 300;  // Longer debounce for track changes

bool volumeUpPressed = false;
bool volumeDownPressed = false;
bool nextTrackPressed = false;
bool prevTrackPressed = false;

// Smooth volume transition variables
unsigned long lastVolumeUpdate = 0;
const unsigned long VOLUME_UPDATE_INTERVAL = 10;  // Update every 10ms

// Debug and status variables
unsigned long lastStatusPrint = 0;
const unsigned long STATUS_PRINT_INTERVAL = 5000;  // Print status every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 Multiple Audio Streaming Player ===");

  // Setup control pins
  pinMode(VOLUME_UP_PIN, INPUT_PULLUP);
  pinMode(VOLUME_DOWN_PIN, INPUT_PULLUP);
  pinMode(NEXT_TRACK_PIN, INPUT_PULLUP);
  pinMode(PREV_TRACK_PIN, INPUT_PULLUP);

  // Connect to WiFi
  Serial.println("[WIFI] Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[WIFI] WiFi connected!");
  Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());

  // Setup I2S audio output
  Serial.println("[AUDIO] Initializing I2S...");
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 27);  // BCLK, LRC, DIN
  out->SetGain(currentVolume); // Set initial volume
  
  // Create MP3 decoder
  mp3 = new AudioGeneratorMP3();

  Serial.println("\n=== HARDWARE CONTROLS ===");
  Serial.printf("GPIO%d - Volume Up\n", VOLUME_UP_PIN);
  Serial.printf("GPIO%d - Volume Down\n", VOLUME_DOWN_PIN);
  Serial.printf("GPIO%d - NEXT TRACK (Forward) ⏭️\n", NEXT_TRACK_PIN);
  Serial.printf("GPIO%d - PREVIOUS TRACK (Backward) ⏮️\n", PREV_TRACK_PIN);
  Serial.println("\n=== WIRING GUIDE ===");
  Serial.println("Connect buttons between GPIO pins and GND:");
  Serial.printf("• Volume Up:     GPIO%d ↔ GND\n", VOLUME_UP_PIN);
  Serial.printf("• Volume Down:   GPIO%d ↔ GND\n", VOLUME_DOWN_PIN);
  Serial.printf("• NEXT Track:    GPIO%d ↔ GND  ⏭️\n", NEXT_TRACK_PIN);
  Serial.printf("• PREVIOUS Track: GPIO%d ↔ GND  ⏮️\n", PREV_TRACK_PIN);
  Serial.println("(All pins use internal pullup resistors)");
  Serial.println("1-6: Play specific track");
  Serial.println("next/n: Next track");
  Serial.println("prev/p: Previous track");
  Serial.println("stop: Stop playback");
  Serial.println("vol: Show volume status");
  Serial.println("up/down: Volume control");
  Serial.println("mute: Toggle mute");
  Serial.println("list: List all tracks");
  
  Serial.printf("\nTotal tracks available: %d\n", TOTAL_STREAMS);
  Serial.printf("Initial Volume: %.0f%%\n", currentVolume * 100);
  
  // Start playing first track
  playTrack(currentTrack);
}

void playTrack(int trackIndex) {
  if (trackIndex < 0 || trackIndex >= TOTAL_STREAMS) {
    Serial.printf("[ERROR] Invalid track index: %d\n", trackIndex);
    return;
  }

  Serial.printf("[TRACK] Switching to track %d: %s\n", trackIndex + 1, streamURLs[trackIndex]);

  // Stop current playback
  if (mp3 && mp3->isRunning()) {
    mp3->stop();
  }
  
  // Clean up previous stream
  if (file) {
    delete file;
    file = nullptr;
  }

  delay(200);  // Allow cleanup

  // Create new stream
  file = new AudioFileSourceICYStream(streamURLs[trackIndex]);
  
  if (!file) {
    Serial.println("[ERROR] Failed to create audio stream");
    return;
  }

  // Start new track
  if (mp3->begin(file, out)) {
    currentTrack = trackIndex;
    out->SetGain(currentVolume);  // Restore volume
    Serial.printf("[TRACK] Now playing: Track %d (Volume: %.0f%%)\n", currentTrack + 1, currentVolume * 100);
  } else {
    Serial.println("[ERROR] Failed to start track playback");
    delete file;
    file = nullptr;
  }
}

void handleTrackButtons() {
  unsigned long currentTime = millis();
  
  // Read track button states (inverted because of INPUT_PULLUP)
  bool nextState = !digitalRead(NEXT_TRACK_PIN);
  bool prevState = !digitalRead(PREV_TRACK_PIN);

  // Handle NEXT TRACK button (Forward)
  if (nextState && !nextTrackPressed && (currentTime - lastNextTrackPress >= TRACK_DEBOUNCE)) {
    nextTrackPressed = true;
    lastNextTrackPress = currentTime;
    
    int nextTrack = (currentTrack + 1) % TOTAL_STREAMS;
    Serial.printf("[BUTTON] ⏭️  NEXT TRACK: %d → %d (%s)\n", 
                  currentTrack + 1, nextTrack + 1, 
                  (nextTrack == 0) ? "wrapping to first" : "advancing");
    trackChangeRequested = true;
    requestedTrack = nextTrack;
  } else if (!nextState) {
    nextTrackPressed = false;
  }

  // Handle PREVIOUS TRACK button (Backward)  
  if (prevState && !prevTrackPressed && (currentTime - lastPrevTrackPress >= TRACK_DEBOUNCE)) {
    prevTrackPressed = true;
    lastPrevTrackPress = currentTime;
    
    int prevTrack = (currentTrack - 1 + TOTAL_STREAMS) % TOTAL_STREAMS;
    Serial.printf("[BUTTON] ⏮️  PREVIOUS TRACK: %d → %d (%s)\n", 
                  currentTrack + 1, prevTrack + 1, 
                  (prevTrack == TOTAL_STREAMS - 1) ? "wrapping to last" : "going back");
    trackChangeRequested = true;
    requestedTrack = prevTrack;
  } else if (!prevState) {
    prevTrackPressed = false;
  }
}

void handleVolumeButtons() {
  unsigned long currentTime = millis();
  
  // Read button states (inverted because of INPUT_PULLUP)
  bool volUpState = !digitalRead(VOLUME_UP_PIN);
  bool volDownState = !digitalRead(VOLUME_DOWN_PIN);

  // Handle Volume Up button
  if (volUpState && !volumeUpPressed) {
    volumeUpPressed = true;
    lastVolumeUpPress = currentTime;
    if (targetVolume < MAX_VOLUME) {
      targetVolume = min(MAX_VOLUME, targetVolume + VOLUME_STEP * 3.0f);
      Serial.printf("[VOLUME] Up: %.0f%% (Track %d)\n", targetVolume * 100, currentTrack + 1);
    }
  } else if (volUpState && volumeUpPressed && (currentTime - lastVolumeUpPress >= VOLUME_REPEAT_DELAY)) {
    lastVolumeUpPress = currentTime;
    if (targetVolume < MAX_VOLUME) {
      targetVolume = min(MAX_VOLUME, targetVolume + VOLUME_STEP * 2.0f);
      Serial.printf("[VOLUME] Up: %.0f%%\n", targetVolume * 100);
    }
  } else if (!volUpState) {
    volumeUpPressed = false;
  }

  // Handle Volume Down button
  if (volDownState && !volumeDownPressed) {
    volumeDownPressed = true;
    lastVolumeDownPress = currentTime;
    if (targetVolume > MIN_VOLUME) {
      targetVolume = max(MIN_VOLUME, targetVolume - VOLUME_STEP * 3.0f);
      Serial.printf("[VOLUME] Down: %.0f%% (Track %d)\n", targetVolume * 100, currentTrack + 1);
    }
  } else if (volDownState && volumeDownPressed && (currentTime - lastVolumeDownPress >= VOLUME_REPEAT_DELAY)) {
    lastVolumeDownPress = currentTime;
    if (targetVolume > MIN_VOLUME) {
      targetVolume = max(MIN_VOLUME, targetVolume - VOLUME_STEP * 2.0f);
      Serial.printf("[VOLUME] Down: %.0f%%\n", targetVolume * 100);
    }
  } else if (!volDownState) {
    volumeDownPressed = false;
  }
}

void updateVolume() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastVolumeUpdate >= VOLUME_UPDATE_INTERVAL) {
    lastVolumeUpdate = currentTime;
    
    // Smooth volume transition
    if (abs(currentVolume - targetVolume) > 0.001f) {
      if (currentVolume < targetVolume) {
        currentVolume = min(targetVolume, currentVolume + VOLUME_STEP);
      } else if (currentVolume > targetVolume) {
        currentVolume = max(targetVolume, currentVolume - VOLUME_STEP);
      }
      
      // Apply the volume change to the audio output
      if (out) {
        out->SetGain(currentVolume);
      }
    }
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      Serial.printf("[CMD] Received: '%s'\n", input.c_str());
      
      // Track selection (1-6)
      if (isDigit(input.charAt(0))) {
        int track = input.toInt();
        if (track >= 1 && track <= TOTAL_STREAMS) {
          trackChangeRequested = true;
          requestedTrack = track - 1;
        } else {
          Serial.printf("[ERROR] Invalid track. Use 1-%d\n", TOTAL_STREAMS);
        }
      }
      // Next track
      else if (input.equalsIgnoreCase("next") || input.equalsIgnoreCase("n")) {
        int nextTrack = (currentTrack + 1) % TOTAL_STREAMS;
        trackChangeRequested = true;
        requestedTrack = nextTrack;
        Serial.printf("[CMD] Next track: %d\n", nextTrack + 1);
      }
      // Previous track
      else if (input.equalsIgnoreCase("prev") || input.equalsIgnoreCase("p")) {
        int prevTrack = (currentTrack - 1 + TOTAL_STREAMS) % TOTAL_STREAMS;
        trackChangeRequested = true;
        requestedTrack = prevTrack;
        Serial.printf("[CMD] Previous track: %d\n", prevTrack + 1);
      }
      // Stop playback
      else if (input.equalsIgnoreCase("stop")) {
        if (mp3 && mp3->isRunning()) {
          mp3->stop();
          Serial.println("[CMD] Playback stopped");
        }
      }
      // Volume status
      else if (input.equalsIgnoreCase("vol")) {
        Serial.printf("[STATUS] Track: %d/%d, Volume: %.0f%%, Target: %.0f%%\n", 
                      currentTrack + 1, TOTAL_STREAMS, currentVolume * 100, targetVolume * 100);
      }
      // Volume up
      else if (input.equalsIgnoreCase("up")) {
        if (targetVolume < MAX_VOLUME) {
          targetVolume = min(MAX_VOLUME, targetVolume + 0.1f);
          Serial.printf("[CMD] Volume up: %.0f%%\n", targetVolume * 100);
        }
      }
      // Volume down
      else if (input.equalsIgnoreCase("down")) {
        if (targetVolume > MIN_VOLUME) {
          targetVolume = max(MIN_VOLUME, targetVolume - 0.1f);
          Serial.printf("[CMD] Volume down: %.0f%%\n", targetVolume * 100);
        }
      }
      // List tracks
      else if (input.equalsIgnoreCase("list")) {
        Serial.println("[TRACKS] Available tracks:");
        for (int i = 0; i < TOTAL_STREAMS; i++) {
          Serial.printf("  %d: %s %s\n", i + 1, streamURLs[i], 
                        (i == currentTrack) ? "← CURRENT" : "");
        }
      }
      else {
        Serial.printf("[ERROR] Unknown command: '%s'\n", input.c_str());
      }
    }
  }
}

void loop() {
  unsigned long currentTime = millis();
  
  // Handle all button inputs
  handleTrackButtons();
  handleVolumeButtons();
  
  // Update volume smoothly
  updateVolume();
  
  // Handle serial commands
  handleSerialCommands();
  
  // Handle track changes
  if (trackChangeRequested) {
    trackChangeRequested = false;
    playTrack(requestedTrack);
  }
  
  // Handle audio streaming
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      Serial.printf("[AUDIO] Track %d ended\n", currentTrack + 1);
      
      // Auto-advance to next track
      int nextTrack = (currentTrack + 1) % TOTAL_STREAMS;
      Serial.printf("[AUTO] Auto-advancing to track %d\n", nextTrack + 1);
      playTrack(nextTrack);
    }
  } else {
    // Try to restart current track if not running
    Serial.printf("[AUDIO] Restarting track %d\n", currentTrack + 1);
    playTrack(currentTrack);
  }
  
  // Print status periodically
  if (currentTime - lastStatusPrint >= STATUS_PRINT_INTERVAL) {
    lastStatusPrint = currentTime;
    Serial.printf("[STATUS] Track: %d/%d, Volume: %.0f%%, Running: %s\n", 
                  currentTrack + 1, TOTAL_STREAMS, currentVolume * 100,
                  (mp3 && mp3->isRunning()) ? "YES" : "NO");
  }
  
  delay(10);  // Small delay for stability
}