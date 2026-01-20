/*
 * ESP-NOW Communication Functions
 * 
 * Handles ESP-NOW setup, channel scanning, and data transmission
 */

#ifndef ESPNOW_COMM_H
#define ESPNOW_COMM_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"

// Global variables for ESP-NOW status
extern int detectedChannel;
extern bool dataSent;
extern bool sendSuccess;

// Callback when data is sent
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);

// Scan WiFi channels to find the Cloud Node
// Returns the channel number if found, or 0 if not found
int scanForCloudNode();

// Initialize ESP-NOW and set up the peer connection
bool initializeESPNOW();

// Send sensor data to the Cloud Node
// Returns true if send was successful, false otherwise
bool sendSensorData(struct_message &data);

#endif // ESPNOW_COMM_H
