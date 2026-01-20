/*
 * BLE WiFi Provisioning Implementation
 */

#include "provisioning.h"

// Global variables
bool provisioningRequested = false;
bool deviceConnected = false;
String wifiSSID = "";
String wifiPassword = "";

// BLE Objects
BLEServer* pProvisioningServer = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;

// Preferences for storing WiFi credentials
Preferences preferences;

// Connection callbacks
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE Client connected for provisioning");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("BLE Client disconnected");
      // Restart advertising
      BLEDevice::startAdvertising();
    }
};

// SSID Characteristic callback
class SSIDCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        wifiSSID = String(value.c_str());
        Serial.print("Received SSID: ");
        Serial.println(wifiSSID);
      }
    }
};

// Password Characteristic callback
class PasswordCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        wifiPassword = String(value.c_str());
        Serial.println("Received Password: ****");
        
        // Trigger provisioning when both SSID and password are received
        if (wifiSSID.length() > 0) {
          provisioningRequested = true;
        }
      }
    }
};

void updateProvisioningStatus(String status) {
  if (pStatusCharacteristic != nullptr) {
    pStatusCharacteristic->setValue(status.c_str());
    pStatusCharacteristic->notify();
    Serial.print("Provisioning Status: ");
    Serial.println(status);
  }
}

void initializeProvisioning() {
  Serial.println("Initializing BLE Provisioning Service...");

  // Create BLE Device (reuse existing BLE if already initialized)
  if (!BLEDevice::getInitialized()) {
    BLEDevice::init("ESP32-IOT-Device");
  }

  // Create BLE Server
  pProvisioningServer = BLEDevice::createServer();
  pProvisioningServer->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  BLEService* pService = pProvisioningServer->createService(SERVICE_UUID);

  // Create SSID Characteristic
  BLECharacteristic* pSSIDCharacteristic = pService->createCharacteristic(
    SSID_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pSSIDCharacteristic->setCallbacks(new SSIDCallbacks());

  // Create Password Characteristic
  BLECharacteristic* pPasswordCharacteristic = pService->createCharacteristic(
    PASSWORD_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pPasswordCharacteristic->setCallbacks(new PasswordCallbacks());

  // Create Status Characteristic
  pStatusCharacteristic = pService->createCharacteristic(
    STATUS_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pStatusCharacteristic->addDescriptor(new BLE2902());
  pStatusCharacteristic->setValue("idle");

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("✓ BLE Provisioning service started");
  Serial.println("✓ Device is ready for WiFi provisioning via IoT Dashboard");
}

bool connectToWiFi(String ssid, String password) {
  Serial.println("Attempting to connect to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  updateProvisioningStatus("connecting");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    updateProvisioningStatus("connected");
    
    // Save credentials
    saveWiFiCredentials(ssid, password);
    
    return true;
  } else {
    Serial.println("✗ Failed to connect to WiFi");
    updateProvisioningStatus("failed");
    return false;
  }
}

bool connectToStoredWiFi() {
  String ssid, password;
  if (getStoredWiFiCredentials(ssid, password)) {
    Serial.println("Found stored WiFi credentials, attempting connection...");
    return connectToWiFi(ssid, password);
  }
  return false;
}

void handleProvisioning() {
  if (provisioningRequested) {
    provisioningRequested = false;
    
    if (connectToWiFi(wifiSSID, wifiPassword)) {
      Serial.println("✓ Provisioning successful!");
      // WiFi connected, can now proceed with normal operation
    } else {
      Serial.println("✗ Provisioning failed - WiFi connection unsuccessful");
    }
  }
}

bool hasStoredWiFiCredentials() {
  preferences.begin("wifi", true); // Read-only mode
  bool hasSSID = preferences.isKey("ssid");
  bool hasPassword = preferences.isKey("password");
  preferences.end();
  return hasSSID && hasPassword;
}

void clearStoredWiFiCredentials() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();
  Serial.println("WiFi credentials cleared");
}

bool getStoredWiFiCredentials(String& ssid, String& password) {
  preferences.begin("wifi", true); // Read-only mode
  if (preferences.isKey("ssid") && preferences.isKey("password")) {
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    preferences.end();
    return (ssid.length() > 0);
  }
  preferences.end();
  return false;
}

void saveWiFiCredentials(String ssid, String password) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  Serial.println("✓ WiFi credentials saved to NVS");
}
