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

#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-56789abcdef1"

// Function declarations
void handleWiFiConfig(String wifiData);
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
    
    Serial.println("WiFi credentials received:");
    Serial.println("SSID: " + ssid);
    Serial.println("Attempting to connect...");
    
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
      String response = "WiFi connection failed! Check credentials.";
      pCharacteristic->setValue(response.c_str());
      pCharacteristic->notify();
      
      Serial.println("WiFi connection failed!");
    }
  } else {
    String response = "Invalid format! Use: WIFI:SSID,PASSWORD";
    pCharacteristic->setValue(response.c_str());
    pCharacteristic->notify();
    Serial.println("Invalid WiFi format received");
  }
}

bool connectToWiFi() {
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  return WiFi.status() == WL_CONNECTED;
}

void sendStatus() {
  String status = "WiFi: ";
  if (WiFi.status() == WL_CONNECTED) {
    status += "Connected (" + WiFi.localIP().toString() + ")";
  } else {
    status += "Disconnected";
  }
  
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
  if (wifiConnected) {
    // WiFi is connected - you can do web requests, etc.
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 30000) { // Print every 30 seconds
      Serial.println("WiFi Status: Connected to " + ssid + " | IP: " + WiFi.localIP().toString());
      lastPrint = millis();
    }
  }
  
  delay(1000);
}