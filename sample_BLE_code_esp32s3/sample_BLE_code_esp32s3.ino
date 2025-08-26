#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-56789abcdef1"

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
      
      if (receivedData.length() > 0) {
        Serial.println("Received: " + receivedData);
        
        String response = "Got: " + receivedData;
        pCharacteristic->setValue(response.c_str());
        pCharacteristic->notify();
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32-S3 BLE...");

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
  Serial.println("Use a BLE app to connect and send data");
}

void loop() {
  delay(1000);
}