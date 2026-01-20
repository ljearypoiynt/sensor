/*
 * BLE WiFi Provisioning Implementation
 */

#include "provisioning.h"
#include "config.h"
#include "espnow_comm.h"

// Global variables
bool provisioningRequested = false;
bool deviceConnected = false;
String wifiSSID = "";
String wifiPassword = "";

// BLE Objects
BLEServer* pProvisioningServer = nullptr;
BLECharacteristic* pStatusCharacteristic = nullptr;
BLECharacteristic* pDeviceInfoCharacteristic = nullptr;
BLECharacteristic* pPropertiesCharacteristic = nullptr;

// Preferences for storing WiFi credentials
Preferences preferences;

// Connection callbacks
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("BLE Client connected for provisioning");
      // Send device info when client connects
      delay(500); // Small delay to ensure connection is stable
      sendDeviceInfo();
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
      String value = pCharacteristic->getValue();
      if (value.length() > 0) {
        wifiSSID = value;
        Serial.print("Received SSID: ");
        Serial.println(wifiSSID);
      }
    }
};

// Password Characteristic callback
class PasswordCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) {
        wifiPassword = value;
        Serial.println("Received Password: ****");
        
        // Trigger provisioning when both SSID and password are received
        if (wifiSSID.length() > 0) {
          provisioningRequested = true;
        }
      }
    }
};

// Properties Characteristic callback
class PropertiesCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
      String value = pCharacteristic->getValue();
      if (value.length() > 0) {
        Serial.print("Received Properties JSON: ");
        Serial.println(value);
        
        // Parse JSON: {"minDistance":20.0,"maxDistance":120.0,"refreshRate":300,"totalLitres":900.0,"cloudNodeMAC":"0C:4E:A0:4D:54:8C"}
        // Simple JSON parsing (Arduino doesn't have built-in JSON, so we parse manually)
        float minDist = 0, maxDist = 0, totalLitres = 0;
        uint32_t refreshRate = 0;
        uint8_t cloudMAC[6] = {0};
        bool hasCloudMAC = false;
        
        int minDistIdx = value.indexOf("\"minDistance\":");
        int maxDistIdx = value.indexOf("\"maxDistance\":");
        int refreshIdx = value.indexOf("\"refreshRate\":");
        int litresIdx = value.indexOf("\"totalLitres\":");
        int cloudMACIdx = value.indexOf("\"cloudNodeMAC\":");
        
        if (minDistIdx != -1) {
          int startIdx = minDistIdx + 14; // Length of "minDistance":
          int endIdx = value.indexOf(',', startIdx);
          if (endIdx == -1) endIdx = value.indexOf('}', startIdx);
          minDist = value.substring(startIdx, endIdx).toFloat();
        }
        
        if (maxDistIdx != -1) {
          int startIdx = maxDistIdx + 14; // Length of "maxDistance":
          int endIdx = value.indexOf(',', startIdx);
          if (endIdx == -1) endIdx = value.indexOf('}', startIdx);
          maxDist = value.substring(startIdx, endIdx).toFloat();
        }
        
        if (refreshIdx != -1) {
          int startIdx = refreshIdx + 14; // Length of "refreshRate":
          int endIdx = value.indexOf(',', startIdx);
          if (endIdx == -1) endIdx = value.indexOf('}', startIdx);
          refreshRate = value.substring(startIdx, endIdx).toInt();
        }
        
        if (litresIdx != -1) {
          int startIdx = litresIdx + 14; // Length of "totalLitres":
          int endIdx = value.indexOf(',', startIdx);
          if (endIdx == -1) endIdx = value.indexOf('}', startIdx);
          totalLitres = value.substring(startIdx, endIdx).toFloat();
        }
        
        if (cloudMACIdx != -1) {
          int startIdx = value.indexOf('"', cloudMACIdx + 15) + 1; // Find opening quote
          int endIdx = value.indexOf('"', startIdx); // Find closing quote
          String macStr = value.substring(startIdx, endIdx);
          macStr.replace(":", ""); // Remove colons
          macStr.toUpperCase();
          
          // Parse hex MAC address
          if (macStr.length() == 12) {
            for (int i = 0; i < 6; i++) {
              String byteStr = macStr.substring(i * 2, i * 2 + 2);
              cloudMAC[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            }
            hasCloudMAC = true;
          }
        }
        
        // Validate values
        if (minDist > 0 && maxDist > 0 && refreshRate > 0 && totalLitres > 0 && minDist < maxDist) {
          saveDeviceProperties(minDist, maxDist, refreshRate, totalLitres, hasCloudMAC ? cloudMAC : nullptr);
          updatePropertiesStatus("properties_updated");
          Serial.println("✓ Properties saved successfully");
          
          // Send updated device info
          delay(100);
          sendDeviceInfo();
        } else {
          updatePropertiesStatus("properties_error");
          Serial.println("✗ Invalid property values received");
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

void updatePropertiesStatus(String status) {
  if (pPropertiesCharacteristic != nullptr) {
    pPropertiesCharacteristic->setValue(status.c_str());
    pPropertiesCharacteristic->notify();
    Serial.print("Properties Status: ");
    Serial.println(status);
  }
}

void sendDeviceInfo() {
  if (pDeviceInfoCharacteristic != nullptr) {
    // Get MAC address
    String macAddress = WiFi.macAddress();
    
    // Format cloud node MAC address
    String cloudMAC = "";
    for (int i = 0; i < 6; i++) {
      if (cloudNodeAddress[i] < 16) cloudMAC += "0";
      cloudMAC += String(cloudNodeAddress[i], HEX);
      if (i < 5) cloudMAC += ":";
    }
    cloudMAC.toUpperCase();
    
    // Create JSON with device info
    String deviceInfo = "{";
    deviceInfo += "\"macAddress\":\"" + macAddress + "\",";
    deviceInfo += "\"deviceType\":\"tank_meter\",";
    deviceInfo += "\"properties\":{";
    deviceInfo += "\"minDistance\":" + String(fullDistanceCm, 1) + ",";
    deviceInfo += "\"maxDistance\":" + String(emptyDistanceCm, 1) + ",";
    deviceInfo += "\"refreshRate\":" + String(refreshRateSeconds) + ",";
    deviceInfo += "\"totalLitres\":" + String(tankCapacityLitres, 1) + ",";
    deviceInfo += "\"cloudNodeMAC\":\"" + cloudMAC + "\"";
    deviceInfo += "}";
    deviceInfo += "}";
    
    pDeviceInfoCharacteristic->setValue(deviceInfo.c_str());
    pDeviceInfoCharacteristic->notify();
    
    Serial.println("Device info sent to frontend:");
    Serial.println(deviceInfo);
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

  // Create Device Info Characteristic
  pDeviceInfoCharacteristic = pService->createCharacteristic(
    DEVICE_INFO_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pDeviceInfoCharacteristic->addDescriptor(new BLE2902());
  pDeviceInfoCharacteristic->setValue("{}");

  // Create Properties Characteristic (for receiving updates from frontend)
  pPropertiesCharacteristic = pService->createCharacteristic(
    PROPERTIES_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pPropertiesCharacteristic->addDescriptor(new BLE2902());
  pPropertiesCharacteristic->setCallbacks(new PropertiesCallbacks());
  pPropertiesCharacteristic->setValue("idle");

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

// Property storage functions
void saveDeviceProperties(float minDist, float maxDist, uint32_t refreshRate, float totalLitres, uint8_t* cloudMAC) {
  preferences.begin("device", false);
  preferences.putFloat("minDist", minDist);
  preferences.putFloat("maxDist", maxDist);
  preferences.putUInt("refreshRate", refreshRate);
  preferences.putFloat("totalLitres", totalLitres);
  preferences.end();
  
  // Update global variables
  fullDistanceCm = minDist;
  emptyDistanceCm = maxDist;
  refreshRateSeconds = refreshRate;
  tankCapacityLitres = totalLitres;
  
  Serial.println("✓ Device properties saved to NVS:");
  Serial.printf("  Min Distance: %.1f cm\n", minDist);
  Serial.printf("  Max Distance: %.1f cm\n", maxDist);
  Serial.printf("  Refresh Rate: %u seconds\n", refreshRate);
  Serial.printf("  Total Litres: %.1f L\n", totalLitres);
  
  // Save cloud node MAC if provided
  if (cloudMAC != nullptr) {
    saveCloudNodeMAC(cloudMAC);
  }
}

bool loadDeviceProperties() {
  preferences.begin("device", true); // Read-only
  
  if (preferences.isKey("minDist") && preferences.isKey("maxDist") && 
      preferences.isKey("refreshRate") && preferences.isKey("totalLitres")) {
    
    fullDistanceCm = preferences.getFloat("minDist", 20.0f);
    emptyDistanceCm = preferences.getFloat("maxDist", 120.0f);
    refreshRateSeconds = preferences.getUInt("refreshRate", 300);
    tankCapacityLitres = preferences.getFloat("totalLitres", 900.0f);
    
    preferences.end();
    
    Serial.println("✓ Loaded stored device properties:");
    Serial.printf("  Min Distance: %.1f cm\n", fullDistanceCm);
    Serial.printf("  Max Distance: %.1f cm\n", emptyDistanceCm);
    Serial.printf("  Refresh Rate: %u seconds\n", refreshRateSeconds);
    Serial.printf("  Total Litres: %.1f L\n", tankCapacityLitres);
    
    return true;
  }
  
  preferences.end();
  return false;
}

bool hasStoredProperties() {
  preferences.begin("device", true); // Read-only
  bool hasProps = preferences.isKey("minDist") && preferences.isKey("maxDist");
  preferences.end();
  return hasProps;
}

void saveCloudNodeMAC(uint8_t* macAddress) {
  preferences.begin("device", false);
  preferences.putBytes("cloudMAC", macAddress, 6);
  preferences.end();
  
  // Update global cloud node address
  memcpy(cloudNodeAddress, macAddress, 6);
  
  Serial.println("✓ Cloud Node MAC saved:");
  Serial.print("  ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", macAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  // Update ESP-NOW peer if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Updating ESP-NOW peer with new MAC address...");
    if (updateCloudNodePeer(macAddress)) {
      Serial.println("✓ ESP-NOW peer updated successfully");
    } else {
      Serial.println("✗ Failed to update ESP-NOW peer - will retry on next boot");
    }
  }
}

bool loadCloudNodeMAC(uint8_t* macAddress) {
  preferences.begin("device", true); // Read-only
  
  if (preferences.isKey("cloudMAC")) {
    size_t len = preferences.getBytes("cloudMAC", macAddress, 6);
    preferences.end();
    
    if (len == 6) {
      Serial.println("✓ Loaded stored Cloud Node MAC:");
      Serial.print("  ");
      for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", macAddress[i]);
        if (i < 5) Serial.print(":");
      }
      Serial.println();
      return true;
    }
  }
  
  preferences.end();
  return false;
}
