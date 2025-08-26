#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// WiFi Credentials
#define WIFI_SSID "reenanup_2.4G"
#define WIFI_PASSWORD "15772424"

// TFT Setup
TFT_eSPI tft = TFT_eSPI();

// Scene images - 6 colored scenes
const char* imageUrls[] = {
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_1_colored.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_2_colored.jpg3",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_3_colored.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_4_colored.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_5_colored.jpg",
  "https://storage.googleapis.com/storyteller-7ece7.firebasestorage.app/stories/story_1eeeaf93/images/scene_6_colored.jpg"
};

int currentScene = 0;
const int totalScenes = 6;
bool isPlaying = false;
bool lastTouchState = false;

// YouTube-style tap detection
unsigned long firstTapTime = 0;
unsigned long doubleTapDelay = 300;  // 300ms window for double tap
bool waitingForSecondTap = false;

// Download task control
volatile bool downloadInProgress = false;
volatile bool imageReadyToDisplay = false; // set true by downloader when finished
volatile int downloadedSceneIndex = 0;

// Overlay control (non-blocking)
volatile bool overlayActive = false;
unsigned long overlayExpireTime = 0;

// JPEG callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Forward declarations
void startImageDownload(int sceneIndex);
void downloadImageTask(void* param);
bool drawImageFromSPIFFS(); // returns true if drawn

// Draw overlay on top of current image (non-blocking draw from loop)
void drawPlayPauseOverlay() {
  // semi-transparent-ish - can't do alpha easily, so just draw black circle then white border and icon
  tft.fillCircle(160, 120, 50, TFT_BLACK);
  tft.drawCircle(160, 120, 50, TFT_WHITE);
  tft.drawCircle(160, 120, 49, TFT_WHITE);

  if (isPlaying) {
    // PAUSE icon (two vertical bars)
    tft.fillRect(140, 100, 8, 40, TFT_WHITE);
    tft.fillRect(172, 100, 8, 40, TFT_WHITE);
    Serial.println("📺 OVERLAY: PAUSE ICON DRAWN");
  } else {
    // PLAY icon (triangle)
    tft.fillTriangle(145, 100, 145, 140, 175, 120, TFT_WHITE);
    Serial.println("📺 OVERLAY: PLAY ICON DRAWN");
  }
}

// Request overlay show (non-blocking)
void showPlayPauseOverlay() {
  overlayActive = true;
  overlayExpireTime = millis() + 1000; // 1 second display
  Serial.println("Overlay requested (non-blocking)");
}

// Start the downloader FreeRTOS task (won't start another if already downloading)
void startImageDownload(int sceneIndex) {
  if (downloadInProgress) {
    Serial.println("Download already in progress, ignoring request.");
    return;
  }
  downloadInProgress = true;
  downloadedSceneIndex = sceneIndex;
  // Create a task pinned to core 1 (App core). Adjust core if you need.
  BaseType_t res = xTaskCreatePinnedToCore(
    downloadImageTask,
    "DownloadTask",
    8192,          // stack size (bytes)
    (void*)(intptr_t)sceneIndex,
    1,             // priority
    NULL,
    1              // pinned to core 1 (app core)
  );
  if (res != pdPASS) {
    Serial.println("Failed to create download task!");
    downloadInProgress = false;
  } else {
    Serial.printf("Download task started for scene %d\n", sceneIndex + 1);
  }
}

// FreeRTOS task: download JPEG and save to SPIFFS as /temp.jpg
void downloadImageTask(void* param) {
  int sceneIndex = (int)(intptr_t)param;
  Serial.printf("DownloadTask: Starting download for scene %d\n", sceneIndex + 1);

  HTTPClient http;
  WiFiClient* stream = nullptr;
  const char* url = imageUrls[sceneIndex];

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("DownloadTask: HTTP Error: %d\n", httpCode);
    http.end();
    downloadInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  stream = http.getStreamPtr();
  File file = SPIFFS.open("/temp.jpg", FILE_WRITE);
  if (!file) {
    Serial.println("DownloadTask: Failed to open /temp.jpg for writing");
    http.end();
    downloadInProgress = false;
    vTaskDelete(NULL);
    return;
  }

  uint8_t buff[1024];
  int len = http.getSize();
  int downloaded = 0;

  // Read and write in chunks, yield occasionally to let WiFi and RTOS handle background tasks
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      int toRead = (size > (sizeof(buff))) ? sizeof(buff) : size;
      int c = stream->readBytes(buff, toRead);
      if (c > 0) {
        file.write(buff, c);
        downloaded += c;
        if (len > 0) len -= c;
      }
    } else {
      // No data available yet; give other tasks some time.
      vTaskDelay(1); // yield 1 tick (non-blocking for WDT)
    }
    // also allow loop to run on main core often
    taskYIELD();
  }

  file.close();
  http.end();

  Serial.printf("DownloadTask: Downloaded %d bytes for scene %d\n", downloaded, sceneIndex + 1);

  // Mark ready for main loop to draw
  imageReadyToDisplay = true;
  downloadedSceneIndex = sceneIndex;

  // End this task
  downloadInProgress = false;
  vTaskDelete(NULL);
}

// Draw image from SPIFFS on the display. Returns true if file exists and draw attempted.
bool drawImageFromSPIFFS() {
  if (!SPIFFS.exists("/temp.jpg")) {
    Serial.println("drawImageFromSPIFFS: /temp.jpg does not exist");
    return false;
  }

  Serial.println("drawImageFromSPIFFS: Displaying /temp.jpg");
  tft.fillScreen(TFT_BLACK);
  TJpgDec.drawFsJpg(0, 0, "/temp.jpg");

  // Draw scene indicator in top-right corner
  tft.fillRect(250, 10, 60, 25, TFT_BLACK);
  tft.drawRect(250, 10, 60, 25, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(258, 18);
  tft.printf("%d / %d", currentScene + 1, totalScenes);

  return true;
}

// Handle single tap (play/pause)
void handleSingleTap() {
  Serial.println("🎬 SINGLE TAP DETECTED");
  if (isPlaying) {
    isPlaying = false;
    Serial.println("⏸️ PAUSED");
  } else {
    isPlaying = true;
    Serial.println("▶️ PLAYING");
  }
  showPlayPauseOverlay(); // non-blocking
}

// Handle double tap (next image) -> start download for next image
void handleDoubleTap() {
  Serial.println("⏭️ DOUBLE TAP DETECTED");
  int previousScene = currentScene;
  currentScene = (currentScene + 1) % totalScenes;
  Serial.printf("Scene change: %d → %d\n", previousScene + 1, currentScene + 1);

  // Start background download (FreeRTOS task)
  startImageDownload(currentScene);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n=== YouTube-Style Image Viewer (WDT-safe) ===");
  Serial.println("🎬 Touch Controls:");
  Serial.println("   • SINGLE TAP = Play/Pause");
  Serial.println("   • DOUBLE TAP = Next Image");
  Serial.println("===================================");

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Show startup message
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.print("YouTube-Style Viewer");
  tft.setTextSize(1);
  tft.setCursor(50, 130);
  tft.print("Single tap = Play/Pause");
  tft.setCursor(50, 145);
  tft.print("Double tap = Next image");
  tft.setCursor(80, 170);
  tft.print("Connecting...");

  Serial.println("TFT initialized");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  // Wait for connection but yield so watchdog is not triggered
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    if (millis() - wifiStart > 20000) { // timeout 20s
      Serial.println("\nWiFi connect timeout, continuing without WiFi (if offline you'll get HTTP errors).");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" Connected!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  }

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS failed");
    // we continue but file ops will fail
  } else {
    Serial.println("SPIFFS ready");
  }

  // Initialize JPEG decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  Serial.println("JPEG decoder ready");

  // Start first image download in background
  startImageDownload(currentScene);

  Serial.println("===================================");
  Serial.println("✅ Ready! Start tapping the screen");
  Serial.println("===================================");
}

void loop() {
  // 1) Handle touch input (non-blocking)
  uint16_t x = 0, y = 0;
  bool touching = tft.getTouch(&x, &y);

  // Detect new touch (touch started)
  if (touching && !lastTouchState) {
    unsigned long currentTime = millis();
    Serial.printf("👆 TAP at (%d, %d) - Time: %lu\n", x, y, currentTime);

    if (waitingForSecondTap) {
      unsigned long timeSinceFirstTap = currentTime - firstTapTime;
      if (timeSinceFirstTap <= doubleTapDelay) {
        // Valid double tap!
        waitingForSecondTap = false;
        handleDoubleTap();
      } else {
        // Too slow, treat as first tap of a new possible double tap
        firstTapTime = currentTime;
        waitingForSecondTap = true;
      }
    } else {
      // This is the first tap
      firstTapTime = currentTime;
      waitingForSecondTap = true;
    }
  }

  lastTouchState = touching;

  // Check single tap timeout (non-blocking)
  if (waitingForSecondTap && (millis() - firstTapTime > doubleTapDelay)) {
    waitingForSecondTap = false;
    handleSingleTap();
  }

  // 2) If download finished, display it (this happens on the main loop to safely call TJpgDec/TFT)
  if (imageReadyToDisplay) {
    imageReadyToDisplay = false;
    // Draw the image we just downloaded
    if (!drawImageFromSPIFFS()) {
      Serial.println("Could not draw image from SPIFFS after download.");
    } else {
      Serial.printf("Scene %d displayed.\n", downloadedSceneIndex + 1);
    }
  }

  // 3) Manage overlay drawing/expiry (non-blocking)
  if (overlayActive) {
    // If overlay was just activated, draw it now
    if (millis() < overlayExpireTime) {
      drawPlayPauseOverlay();
    } else {
      // overlay expired -> refresh image to remove overlay if possible
      overlayActive = false;
      // Refresh image only if not currently downloading
      if (!downloadInProgress) {
        // Re-draw current image to remove overlay graphics
        if (SPIFFS.exists("/temp.jpg")) {
          TJpgDec.drawFsJpg(0, 0, "/temp.jpg");
          // re-draw scene indicator
          tft.fillRect(250, 10, 60, 25, TFT_BLACK);
          tft.drawRect(250, 10, 60, 25, TFT_WHITE);
          tft.setTextColor(TFT_WHITE);
          tft.setTextSize(1);
          tft.setCursor(258, 18);
          tft.printf("%d / %d", currentScene + 1, totalScenes);
        }
      }
    }
  }

  // Periodic status update (every 15s) but non-blocking
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 15000) {
    Serial.println("=== STATUS UPDATE ===");
    Serial.printf("Current Scene: %d of %d\n", currentScene + 1, totalScenes);
    Serial.printf("Play State: %s\n", isPlaying ? "PLAYING" : "PAUSED");
    Serial.printf("Download in progress: %s\n", downloadInProgress ? "YES" : "NO");
    Serial.println("Controls: Single tap = Play/Pause, Double tap = Next");
    Serial.println("===================");
    lastStatus = millis();
  }

  // Very short delay to avoid a tight busy loop; this keeps responsiveness and lets WiFi/RTOS run
  delay(10);
}
