/*
 * ESP-NOW Communication Implementation
 */

#include "espnow_comm.h"

int detectedChannel = 0;
bool dataSent = false;
bool sendSuccess = false;

// RTC memory to store last successful channel (survives deep sleep)
RTC_DATA_ATTR int savedChannel = 0;

// Helper function to try a specific channel
int tryChannel(int channel) {
  // Set WiFi channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  // Remove existing peer if any
  esp_now_del_peer(cloudNodeAddress);
  
  // Register peer on this channel
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, cloudNodeAddress, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    // Try sending test data multiple times per channel
    for (int attempt = 0; attempt < 2; attempt++) {
      dataSent = false;
      sendSuccess = false;
      
      // Send a test message
      struct_message testData;
      testData.distance_cm = 0;
      testData.level_percent = 0;
      testData.litres_remaining = 0;
      testData.timestamp = millis();
      
      esp_err_t result = esp_now_send(cloudNodeAddress, (uint8_t *) &testData, sizeof(testData));
      
      if (result == ESP_OK) {
        // Wait for callback with longer timeout
        unsigned long startWait = millis();
        while (!dataSent && (millis() - startWait < 1000)) {
          delay(10);
        }
        
        if (sendSuccess) {
          LOG("Cloud Node found!");
          // Don't remove peer - we'll keep using it
          return channel;
        }
      }
      delay(200);  // Wait between attempts
    }
    
    // Remove peer for next attempt
    esp_now_del_peer(cloudNodeAddress);
  }
  
  return 0;  // Failed to connect on this channel
}

void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Delivery Success");
    dataSent = true;
    sendSuccess = true;
  } else {
    Serial.println("Delivery Fail");
    dataSent = true;
    sendSuccess = false;
  }
}

int scanForCloudNode() {
  LOG("Starting channel scan...");
  Serial.print("Looking for Cloud Node MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", cloudNodeAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  // If we have a saved channel from last time, try it first
  if (savedChannel > 0 && savedChannel <= 13) {
    Serial.print("Trying saved channel ");
    Serial.print(savedChannel);
    Serial.println(" first...");
    
    int result = tryChannel(savedChannel);
    if (result > 0) {
      Serial.println("SUCCESS on saved channel!");
      return result;
    }
    Serial.println("Saved channel failed, scanning all channels...");
  }
  
  // Try common channels first (1, 6, 11), then all others
  int channelsToTry[] = {1, 6, 11, 2, 3, 4, 5, 7, 8, 9, 10, 12, 13};
  int numChannels = sizeof(channelsToTry) / sizeof(channelsToTry[0]);
  
  for (int i = 0; i < numChannels; i++) {
    int channel = channelsToTry[i];
    
    // Skip if this is the saved channel we already tried
    if (channel == savedChannel) {
      continue;
    }
    
    Serial.print("Trying channel ");
    Serial.print(channel);
    Serial.print("... ");
    
    int result = tryChannel(channel);
    if (result > 0) {
      Serial.println("SUCCESS!");
      return result;
    }
    Serial.println("no response");
    
    delay(100);
  }
  
  LOG("Channel scan failed - Cloud Node not found on any channel");
  Serial.println("⚠️ CHECK:");
  Serial.println("  1. Is Cloud Node running and connected to WiFi?");
  Serial.println("  2. Is the MAC address correct?");
  Serial.println("  3. Are devices within range (~100m)?");
  return 0;
}

bool initializeESPNOW() {
  // Init ESP-NOW first
  if (esp_now_init() != ESP_OK) {
    LOG("Error initializing ESP-NOW");
    return false;
  }

  // Register send callback
  esp_now_register_send_cb(OnDataSent);

  // Determine WiFi channel
  if (WIFI_CHANNEL == 0) {
    // Auto-scan for channel
    LOG("Auto-scan mode enabled");
    detectedChannel = scanForCloudNode();
    
    if (detectedChannel == 0) {
      LOG("Failed to find Cloud Node - will retry on next wake");
      // Try channel 1 as fallback
      detectedChannel = 1;
      
      // Set WiFi channel
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(detectedChannel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
      
      // Register peer (Cloud Node) on fallback channel
      esp_now_peer_info_t peerInfo;
      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, cloudNodeAddress, 6);
      peerInfo.channel = detectedChannel;
      peerInfo.encrypt = false;

      // Add peer        
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        LOG("Failed to add peer");
        return false;
      }
    } else {
      // Channel found and peer already added during scan
      LOG("Using detected channel - peer already registered");
      // Save the successful channel for next time
      savedChannel = detectedChannel;
      Serial.print("Saved channel ");
      Serial.print(savedChannel);
      Serial.println(" to RTC memory");
    }
  } else {
    // Use specified channel
    detectedChannel = WIFI_CHANNEL;
    LOG("Using pre-configured channel");
    
    // Set WiFi channel
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(detectedChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    
    // Register peer (Cloud Node) on the specified channel
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, cloudNodeAddress, 6);
    peerInfo.channel = detectedChannel;
    peerInfo.encrypt = false;

    // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      LOG("Failed to add peer");
      return false;
    }
  }
  
  Serial.print("WiFi Channel set to: ");
  Serial.println(detectedChannel);
  
  return true;
}

bool sendSensorData(struct_message &data) {
  Serial.print("Sending data via ESP-NOW on channel ");
  Serial.println(detectedChannel);
  dataSent = false;
  sendSuccess = false;
  
  esp_err_t result = esp_now_send(cloudNodeAddress, (uint8_t *) &data, sizeof(data));
  
  if (result == ESP_OK) {
    LOG("Sent with success");
    
    // Wait for send callback
    unsigned long startWait = millis();
    while (!dataSent && (millis() - startWait < 2000)) {
      delay(10);
    }
    
    // Check if send was actually successful
    if (!sendSuccess) {
      LOG("Send failed - Cloud Node may have changed channels");
      LOG("Rescanning for Cloud Node...");
      
      // Remove the old peer
      esp_now_del_peer(cloudNodeAddress);
      
      // Scan for the Cloud Node on a new channel
      int newChannel = scanForCloudNode();
      
      if (newChannel > 0) {
        detectedChannel = newChannel;
        Serial.print("Found Cloud Node on new channel: ");
        Serial.println(detectedChannel);
        
        // Try sending again on the new channel
        LOG("Retrying send on new channel...");
        dataSent = false;
        sendSuccess = false;
        
        result = esp_now_send(cloudNodeAddress, (uint8_t *) &data, sizeof(data));
        
        if (result == ESP_OK) {
          startWait = millis();
          while (!dataSent && (millis() - startWait < 2000)) {
            delay(10);
          }
          
          if (sendSuccess) {
            LOG("Retry successful!");
            // Save the successful channel for next time
            savedChannel = detectedChannel;
            Serial.print("Saved channel ");
            Serial.print(savedChannel);
            Serial.println(" to RTC memory");
            return true;
          } else {
            LOG("Retry failed");
            return false;
          }
        }
      } else {
        LOG("Could not find Cloud Node on any channel");
        return false;
      }
    } else {
      LOG("Send confirmed successful");
      // Save the successful channel for next time
      if (savedChannel != detectedChannel) {
        savedChannel = detectedChannel;
        Serial.print("Saved channel ");
        Serial.print(savedChannel);
        Serial.println(" to RTC memory");
      }
      return true;
    }
  } else {
    LOG("Error sending the data");
    return false;
  }
  
  return false;
}

bool updateCloudNodePeer(uint8_t* newMacAddress) {
  Serial.println("Updating ESP-NOW peer with new cloud node MAC...");
  
  // Remove the old peer if it exists
  esp_now_del_peer(cloudNodeAddress);
  
  // Copy new MAC address to cloudNodeAddress (already done in saveCloudNodeMAC, but just to be safe)
  memcpy(cloudNodeAddress, newMacAddress, 6);
  
  Serial.print("New Cloud Node MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", cloudNodeAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  
  // Scan for the new cloud node
  int newChannel = scanForCloudNode();
  
  if (newChannel > 0) {
    detectedChannel = newChannel;
    savedChannel = newChannel;
    Serial.print("✓ Found new Cloud Node on channel: ");
    Serial.println(detectedChannel);
    return true;
  } else {
    Serial.println("✗ Could not find new Cloud Node on any channel");
    Serial.println("  ESP-NOW peer will be updated on next reboot");
    return false;
  }
}
