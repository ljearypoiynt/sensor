/*
 * Configuration and Constants
 * 
 * All user-configurable settings and pin definitions
 */

#ifndef CONFIG_H
#define CONFIG_H

// ----------- Bluetooth/Provisioning Configuration -----------
static const unsigned long BT_TIMEOUT_MS = 2ULL * 60ULL * 1000ULL; // 2 minutes
static const char* BT_DEVICE_NAME = "ESP32-Sensor-Node";
static const unsigned long PROVISIONING_TIMEOUT_MS = 5ULL * 60ULL * 1000ULL; // 5 minutes for WiFi provisioning

// ----------- Pins (change to suit your ESP32-C3 Super Mini wiring) -----------
// SR04M-2 board labeled RX/TX but can work in standard trigger/echo mode
// Use RX as TRIG and TX as ECHO (or vice versa)
static const int TRIG_PIN = 4;  // Connect to SR04M-2 RX pin
static const int ECHO_PIN = 5;  // Connect to SR04M-2 TX pin
static const int SENSOR_POWER_PIN = 3;  // NPN transistor base for sensor power control
static const int BATTERY_VOLTAGE_PIN = 0;  // ADC pin for battery voltage via voltage divider

// ----------- Battery Voltage Divider Configuration -----------
// R1 = 100k (to battery), R2 = 100k (to ground)
// Voltage at ADC = Battery_Voltage * (R2 / (R1 + R2)) = Battery_Voltage * 0.5
// Therefore: Battery_Voltage = ADC_Voltage * 2.0
static const float VOLTAGE_DIVIDER_RATIO = 2.0f;

// ----------- Deep Sleep Configuration -----------
static const uint64_t SLEEP_TIME_US = 5ULL * 60ULL * 1000000ULL; // 1 hour

// ----------- Tank calibration (EDIT THESE) -----------
// These values can be updated via BLE from the frontend
// Default values are used if no stored values exist
extern float emptyDistanceCm;
extern float fullDistanceCm;
extern float tankCapacityLitres;
extern uint32_t refreshRateSeconds;

// ----------- Measurement settings -----------
static const int samplesPerUpdate = 7;
static const float speedOfSoundCmPerUs = 0.0343f;

// ----------- ESP-NOW Configuration -----------
// MAC Address of the Cloud ESP32 (EDIT THIS after you get it from Cloud Node)
// You can find it by running the Cloud Node and checking Serial output
extern uint8_t cloudNodeAddress[6]; // REPLACE WITH ACTUAL MAC in ESP32_Sensor_Node.ino

// WiFi channel scanning - will automatically find the correct channel
// Set to 0 to enable auto-scanning, or set to a specific channel (1-13) to skip scanning
#define WIFI_CHANNEL 0  // 0 = auto-scan, or set to specific channel (1-13)

// Structure to send data
typedef struct struct_message {
  float distance_cm;
  float level_percent;
  float litres_remaining;
  uint32_t timestamp;
  float battery_v;
} struct_message;

// Logging macro
#define LOG(msg) do { \
  Serial.print("["); Serial.print(millis()); Serial.print(" ms] "); Serial.println(msg); \
} while (0)

#endif // CONFIG_H
