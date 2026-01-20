/*
 * BLE WiFi Provisioning Functions
 * 
 * Handles BLE-based WiFi credential provisioning for IoT Dashboard
 */

#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <Preferences.h>

// BLE Service and Characteristic UUIDs - Must match frontend
#define SERVICE_UUID        "0000ff00-0000-1000-8000-00805f9b34fb"
#define SSID_CHAR_UUID      "0000ff01-0000-1000-8000-00805f9b34fb"
#define PASSWORD_CHAR_UUID  "0000ff02-0000-1000-8000-00805f9b34fb"
#define STATUS_CHAR_UUID    "0000ff03-0000-1000-8000-00805f9b34fb"
#define DEVICE_INFO_CHAR_UUID "0000ff04-0000-1000-8000-00805f9b34fb"
#define PROPERTIES_CHAR_UUID "0000ff05-0000-1000-8000-00805f9b34fb"

// Provisioning state
extern bool provisioningRequested;
extern bool deviceConnected;

// BLE Objects
extern BLEServer* pProvisioningServer;
extern BLECharacteristic* pStatusCharacteristic;
extern BLECharacteristic* pDeviceInfoCharacteristic;
extern BLECharacteristic* pPropertiesCharacteristic;

// Send device info (MAC address, device type, and properties)
void sendDeviceInfo();

// Property storage functions
void saveDeviceProperties(float minDist, float maxDist, uint32_t refreshRate, float totalLitres, uint8_t* cloudMAC);
bool loadDeviceProperties();
bool hasStoredProperties();
void saveCloudNodeMAC(uint8_t* macAddress);
bool loadCloudNodeMAC(uint8_t* macAddress);

// Initialize BLE provisioning service
void initializeProvisioning();

// Update provisioning status and notify clients
void updateProvisioningStatus(String status);

// Attempt to connect to WiFi with stored credentials
bool connectToStoredWiFi();

// Connect to WiFi with new credentials
bool connectToWiFi(String ssid, String password);

// Handle provisioning in the main loop
void handleProvisioning();

// Check if WiFi credentials are stored
bool hasStoredWiFiCredentials();

// Clear stored WiFi credentials
void clearStoredWiFiCredentials();

// Get stored WiFi credentials
bool getStoredWiFiCredentials(String& ssid, String& password);

// Save WiFi credentials to NVS
void saveWiFiCredentials(String ssid, String password);

// Update properties status response
void updatePropertiesStatus(String status);

#endif // PROVISIONING_H
