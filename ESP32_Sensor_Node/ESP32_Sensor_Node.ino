/*
 * ESP32 Sensor Node - Main File
 * 
 * Reads ultrasonic sensor data and sends it to the Cloud Node via ESP-NOW
 * This ESP32 handles sensor reading and goes into deep sleep to save power
 */

#include <WiFi.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "config.h"
#include "sensor.h"
#include "espnow_comm.h"
#include "provisioning.h"

// Define the cloud node MAC address here (declared extern in config.h)
uint8_t cloudNodeAddress[6] = {0x0C, 0x4E, 0xA0, 0x4D, 0x54, 0x8C}; // REPLACE WITH ACTUAL MAC
//uint8_t cloudNodeAddress[6] = {0x0C, 0x4E, 0xA0, 0x4D, 0x54, 0x78}; //oil2
struct_message sensorData;

// BLE variables
BLEServer* pServer = NULL;
bool btEnabled = false;
unsigned long btStartTime = 0;
bool provisioningMode = false;
unsigned long provisioningStartTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  LOG("Sensor Node Booting");
  Serial.println("========================================");

  // Check reset reason - only enable Bluetooth on cold boot
  esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.print("Reset reason: ");
  switch(reset_reason) {
    case ESP_RST_POWERON:
      Serial.println("Power-on reset (cold boot)");
      
      // Check if we have stored WiFi credentials
      if (hasStoredWiFiCredentials()) {
        Serial.println("Found stored WiFi credentials");
        
        // Try to connect with stored credentials
        if (connectToStoredWiFi()) {
          Serial.println("✓ Connected to WiFi with stored credentials");
          // WiFi is connected, skip provisioning
          
          // Enable basic BLE advertising for pairing only
          BLEDevice::init(BT_DEVICE_NAME);
          pServer = BLEDevice::createServer();
          BLEDevice::startAdvertising();
          btEnabled = true;
          btStartTime = millis();
          Serial.println("✓ BLE ENABLED for pairing (2 minutes)");
        } else {
          Serial.println("✗ Failed to connect with stored credentials");
          Serial.println("Entering provisioning mode...");
          
          // Clear invalid credentials and enter provisioning
          clearStoredWiFiCredentials();
          initializeProvisioning();
          provisioningMode = true;
          provisioningStartTime = millis();
          Serial.println("✓ BLE Provisioning ACTIVE (5 minutes)");
        }
      } else {
        // No stored credentials - enter provisioning mode
        Serial.println("No stored WiFi credentials found");
        Serial.println("Entering provisioning mode...");
        initializeProvisioning();
        provisioningMode = true;
        provisioningStartTime = millis();
        Serial.println("✓ BLE Provisioning ACTIVE (5 minutes)");
      }
      
      Serial.print("Device name: ");
      Serial.println(BT_DEVICE_NAME);
      Serial.println("✓ Device is now visible via BLE");
      Serial.print("Search for '");
      Serial.print(BT_DEVICE_NAME);
      Serial.println("' to connect");
      break;
    case ESP_RST_DEEPSLEEP:
      Serial.println("Wake from deep sleep (Bluetooth disabled)");
      break;
    default:
      Serial.print("Other reset: ");
      Serial.println(reset_reason);
      break;
  }
  Serial.println("========================================");

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);

  // Don't initialize ESP-NOW if we're in provisioning mode without WiFi
  if (!provisioningMode || WiFi.status() == WL_CONNECTED) {
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Print MAC address
    Serial.print("Sensor Node MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    Serial.print("Target Cloud Node MAC: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", cloudNodeAddress[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    Serial.println("⚠️ Make sure this matches your Cloud Node's MAC!");
    Serial.println("========================================");

    // Initialize ESP-NOW and find Cloud Node
    if (!initializeESPNOW()) {
      LOG("ESP-NOW initialization failed");
      ESP.restart();
    }
  } else {
    Serial.println("Skipping ESP-NOW initialization - waiting for WiFi provisioning");
    Serial.println("========================================");
  }

  LOG("Sensor Node Setup complete");
}

void loop() {
  // Handle WiFi provisioning if in provisioning mode
  if (provisioningMode) {
    handleProvisioning();
    
    // Check if provisioning timeout has expired
    if (millis() - provisioningStartTime >= PROVISIONING_TIMEOUT_MS) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Provisioning timeout - no WiFi connection");
        Serial.println("Restarting to retry...");
        delay(1000);
        ESP.restart();
      } else {
        // WiFi connected, exit provisioning mode
        Serial.println("Exiting provisioning mode - WiFi connected");
        provisioningMode = false;
        
        // Initialize ESP-NOW now that we have WiFi
        WiFi.mode(WIFI_STA);
        Serial.print("Sensor Node MAC Address: ");
        Serial.println(WiFi.macAddress());
        
        if (!initializeESPNOW()) {
          LOG("ESP-NOW initialization failed");
          ESP.restart();
        }
      }
    }
    
    // Stay in provisioning loop
    delay(100);
    return;
  }
  
  // Check if BLE timeout has expired (for basic pairing mode)
  if (btEnabled && (millis() - btStartTime >= BT_TIMEOUT_MS)) {
    Serial.println("BLE timeout - disabling BLE");
    BLEDevice::deinit();
    btEnabled = false;
  }

  LOG("Loop start - Reading sensor");

  float batteryVoltage = readBatteryVoltage();
  float d = readSmoothedDistanceCm();
  
  if (isnan(d)) {
    LOG("Distance read FAILED - using fallback value");
    d = 80.0f;
  } else {
    LOG("Distance read OK");
  }

  // Calculate values
  sensorData.distance_cm = d;
  float pct = (emptyDistanceCm - d) / (emptyDistanceCm - fullDistanceCm) * 100.0f;
  
  // Clamp percentage between 0-100%
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  
  sensorData.level_percent = pct;
  sensorData.litres_remaining = (pct / 100.0f) * tankCapacityLitres;
  sensorData.timestamp = millis();
  sensorData.battery_v = batteryVoltage;
  Serial.print("Battery Voltage(V): "); Serial.println(batteryVoltage, 2);
  Serial.print("Distance(cm): "); Serial.println(sensorData.distance_cm);
  Serial.print("Level(%): "); Serial.println(sensorData.level_percent);
  Serial.print("Litres: "); Serial.println(sensorData.litres_remaining);

  // Send sensor data (handles retries and channel rescanning internally)
  sendSensorData(sensorData);

  LOG("Entering deep sleep for 1 hour");
  delay(1000);
  
  //esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  //esp_deep_sleep_start();
}
