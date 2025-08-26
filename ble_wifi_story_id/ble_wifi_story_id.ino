#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool wifiConnected = false;

String ssid = "";
String password = "";
String storyId = "";
bool storyIdReceived = false;

#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-56789abcdef1"

// Function declarations
void handleWiFiConfig(String wifiData);
void handleStoryId(String storyData);
bool connectToWiFi();
void sendStatus();

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE device connected!");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE device disconnected");
      delay(500);
      pServer->startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String receivedData = pCharacteristic->getValue().c_str();
      receivedData.trim();
      
      if (receivedData.length() > 0) {
        Serial.println("Received: " + receivedData);
        
        // Handle WiFi credentials (format: WIFI:SSID,PASSWORD)
        if (receivedData.startsWith("WIFI:")) {
          handleWiFiConfig(receivedData.substring(5));
        }
        // Handle story ID (format: STORY:story_id)
        else if (receivedData.startsWith("STORY:")) {
          handleStoryId(receivedData.substring(6));
        }
        // Handle other commands
        else if (receivedData == "STATUS") {
          sendStatus();
        }
        else if (receivedData == "RESET") {
          pCharacteristic->setValue("Resetting ESP32-S3...");
          pCharacteristic->notify();
          delay(1000);
          ESP.restart();
        }
        else {
          String response = "Unknown command: " + receivedData;
          pCharacteristic->setValue(response.c_str());
          pCharacteristic->notify();
        }
      }
    }
};

void handleWiFiConfig(String wifiData) {
  int commaIndex = wifiData.indexOf(',');
  
  if (commaIndex > 0) {
    ssid = wifiData.substring(0, commaIndex);
    password = wifiData.substring(commaIndex + 1);
    
    // Remove any hidden characters
    ssid.trim();
    password.trim();
    
    Serial.println("WiFi credentials received:");
    Serial.println("SSID: '" + ssid + "' (length: " + String(ssid.length()) + ")");
    Serial.println("Password length: " + String(password.length()));
    
    pCharacteristic->setValue("Connecting to WiFi...");
    pCharacteristic->notify();
    
    // Try to connect to WiFi
    if (connectToWiFi()) {
      String response = "WiFi Connected! IP: " + WiFi.localIP().toString();
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      wifiConnected = true;
      
      Serial.println("WiFi connected successfully!");
      Serial.println("IP address: " + WiFi.localIP().toString());
    } else {
      String response = "WiFi connection failed! Status: " + String(WiFi.status());
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      
      Serial.println("WiFi connection failed! Check credentials and network.");
    }
  } else {
    String response = "Invalid format! Use: WIFI:SSID,PASSWORD";
    pCharacteristic->setValue(response.c_str());
    pCharacteristic->notify();
    Serial.println("Invalid WiFi format received");
  }
}

void handleStoryId(String storyData) {
  storyData.trim();
  
  if (storyData.length() > 0) {
    storyId = storyData;
    storyIdReceived = true;
    
    String response = "Story ID set: " + storyId;
    pCharacteristic->setValue(response.c_str());
    pCharacteristic->notify();
    
    Serial.println("Story ID received: " + storyId);
    
    // Check if fully configured
    if (wifiConnected && storyIdReceived) {
      String fullResponse = "Device fully configured! WiFi + Story ID ready.";
      pCharacteristic->setValue(fullResponse.c_str());
      pCharacteristic->notify();
      Serial.println("Device fully configured!");
    }
  } else {
    String response = "Invalid story ID! Please provide a valid ID.";
    pCharacteristic->setValue(response.c_str());
    pCharacteristic->notify();
    Serial.println("Invalid story ID received");
  }
}

bool connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  Serial.println("SSID: '" + ssid + "'");
  Serial.println("Password length: " + String(password.length()));
  
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {  // Increased to 30 attempts
    delay(1000);  // Increased delay to 1 second
    Serial.print(".");
    Serial.print(WiFi.status());  // Print WiFi status code
    attempts++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected successfully!");
    Serial.println("IP: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("WiFi connection failed!");
    Serial.println("Final status: " + String(WiFi.status()));
    Serial.println("Status codes: 0=IDLE, 1=NO_SSID, 3=CONNECTED, 4=CONNECT_FAILED, 6=DISCONNECTED");
    return false;
  }
}

void sendStatus() {
  String status = "WiFi: ";
  if (WiFi.status() == WL_CONNECTED) {
    status += "Connected (" + WiFi.localIP().toString() + ")";
  } else {
    status += "Disconnected";
  }
  
  status += " | Story ID: ";
  status += storyIdReceived ? storyId : "Not set";
  
  status += " | Free Heap: " + String(ESP.getFreeHeap());
  status += " | Uptime: " + String(millis()/1000) + "s";
  
  pCharacteristic->setValue(status.c_str());
  pCharacteristic->notify();
  
  Serial.println("Status sent: " + status);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-S3 WiFi via BLE...");

  // Initialize BLE
  BLEDevice::init("ESP32-S3");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();
  
  Serial.println("BLE started! Device name: ESP32-S3");
  Serial.println("Send WiFi credentials: WIFI:YourSSID,YourPassword");
  Serial.println("Send Story ID: STORY:your_story_id");
  Serial.println("Other commands: STATUS, RESET");
}

void loop() {
  // Check WiFi connection periodically
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting reconnection...");
    wifiConnected = false;
    
    if (connectToWiFi()) {
      wifiConnected = true;
      Serial.println("WiFi reconnected!");
    }
  }
  
  // Your main application code here
  if (wifiConnected && storyIdReceived) {
    // Both WiFi and Story ID are configured - device is fully ready
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 30000) { // Print every 30 seconds
      Serial.println("Device Status: WiFi Connected | Story ID: " + storyId + " | IP: " + WiFi.localIP().toString());
      lastPrint = millis();
    }
    
    // Add your main application logic here
    // You can now use both WiFi connection and storyId for your application
    
  } else if (wifiConnected) {
    // WiFi connected but waiting for story ID
    static unsigned long lastWarn = 0;
    if (millis() - lastWarn > 30000) {
      Serial.println("WiFi connected, waiting for Story ID...");
      lastWarn = millis();
    }
  }
  
  delay(1000);
}