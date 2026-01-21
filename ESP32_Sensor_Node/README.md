# ESP32 Sensor Node - Tank Meter

A battery-powered ESP32 sensor node for monitoring tank levels using an ultrasonic sensor (SR04M-2). The device uses ESP-NOW for efficient communication with a cloud gateway and features BLE-based WiFi provisioning and remote configuration.

## Features

- **Ultrasonic Tank Level Monitoring** - Measures distance and calculates tank level percentage and litres remaining
- **BLE WiFi Provisioning** - Configure WiFi credentials via Bluetooth Low Energy
- **Remote Property Management** - Update device configuration (tank dimensions, refresh rate, gateway MAC) from frontend
- **ESP-NOW Communication** - Low-latency, energy-efficient data transmission to cloud gateway
- **Automatic Channel Scanning** - Finds the gateway on any WiFi channel (1-13)
- **Persistent Storage** - All settings saved to NVS (Non-Volatile Storage)
- **Deep Sleep Mode** - Ultra-low power consumption between readings
- **Battery Voltage Monitoring** - Tracks battery status via voltage divider
- **Auto-Reconnection** - Recovers from channel changes and connection issues

## Hardware Requirements

### Components
- **ESP32-C3 Super Mini** (or compatible ESP32 board)
- **SR04M-2 Ultrasonic Sensor** (waterproof)
- **NPN Transistor** (for sensor power control)
- **Voltage Divider** (2x 100kΩ resistors for battery monitoring)
- **Battery** (LiPo or similar)

### Pin Configuration

```cpp
TRIG_PIN = 4              // Ultrasonic trigger pin (connect to SR04M-2 RX)
ECHO_PIN = 5              // Ultrasonic echo pin (connect to SR04M-2 TX)
SENSOR_POWER_PIN = 3      // NPN transistor base for sensor power
BATTERY_VOLTAGE_PIN = 0   // ADC pin for battery voltage
```

## Device Properties

### Tank Configuration
These properties can be configured via BLE from your frontend application:

| Property | Description | Default | Unit |
|----------|-------------|---------|------|
| **minDistance** | Distance when tank is full | 20.0 | cm |
| **maxDistance** | Distance when tank is empty | 120.0 | cm |
| **refreshRate** | Time between sensor readings | 300 | seconds |
| **totalLitres** | Tank capacity | 900.0 | litres |
| **cloudNodeMAC** | Gateway/Cloud Node MAC address | 0C:4E:A0:4D:54:8C | hex |

## WiFi Provisioning via BLE

### First Boot
On first power-on, the device:
1. Enters **provisioning mode** for 5 minutes
2. Advertises as `ESP32-Sensor-Node` via BLE
3. Waits for WiFi credentials from frontend

### Subsequent Boots
- Connects using stored WiFi credentials
- Enables BLE for **2 minutes** for pairing/configuration only
- Automatically connects to cloud gateway via ESP-NOW

### Provisioning Mode Re-entry
Device returns to provisioning mode if:
- Stored WiFi credentials are invalid
- Cannot connect to stored WiFi network

## BLE Characteristics

### Service UUID
```
0000ff00-0000-1000-8000-00805f9b34fb
```

### Characteristics

| Characteristic | UUID | Type | Description |
|----------------|------|------|-------------|
| **SSID** | 0000ff01... | WRITE | WiFi network name |
| **Password** | 0000ff02... | WRITE | WiFi password |
| **Status** | 0000ff03... | READ/NOTIFY | Provisioning status |
| **Device Info** | 0000ff04... | READ/NOTIFY | Device MAC, type, and properties |
| **Properties** | 0000ff05... | WRITE/NOTIFY | Update device properties |

## JSON Data Formats

### Device Info (Sent to Frontend)
```json
{
  "macAddress": "XX:XX:XX:XX:XX:XX",
  "deviceType": "tank_meter",
  "properties": {
    "minDistance": 20.0,
    "maxDistance": 120.0,
    "refreshRate": 300,
    "totalLitres": 900.0,
    "cloudNodeMAC": "0C:4E:A0:4D:54:8C"
  }
}
```

### Properties Update (Received from Frontend)
```json
{
  "minDistance": 20.0,
  "maxDistance": 120.0,
  "refreshRate": 300,
  "totalLitres": 900.0,
  "cloudNodeMAC": "0C:4E:A0:4D:54:8C"
}
```

### Status Responses
- `"idle"` - Ready for configuration
- `"connecting"` - Attempting WiFi connection
- `"connected"` - WiFi connected successfully
- `"failed"` - WiFi connection failed
- `"properties_updated"` - Properties saved successfully
- `"properties_error"` - Invalid property values

## ESP-NOW Communication

### Data Structure Sent to Gateway
```cpp
struct_message {
  float distance_cm;          // Raw distance measurement
  float level_percent;        // Tank level (0-100%)
  float litres_remaining;     // Calculated volume
  uint32_t timestamp;         // Measurement timestamp (ms)
  float battery_v;            // Battery voltage
}
```

### Channel Management
- **Auto-scanning**: Searches channels 1-13 to find gateway
- **RTC Memory**: Saves last successful channel across deep sleep
- **Auto-recovery**: Rescans if gateway changes channels
- **Priority channels**: Tests 1, 6, 11 first (common WiFi channels)

## Setup Instructions

### 1. Hardware Setup
1. Connect SR04M-2 sensor:
   - RX → GPIO 4 (TRIG)
   - TX → GPIO 5 (ECHO)
   - VCC → Transistor collector
   - GND → Ground
2. Connect transistor for sensor power:
   - Base → GPIO 3 (via 1kΩ resistor)
   - Emitter → Ground
   - Collector → Sensor VCC
3. Create voltage divider for battery:
   - Battery+ → 100kΩ → GPIO 0 → 100kΩ → Ground

### 2. Software Setup
1. Install Arduino IDE or PlatformIO
2. Install ESP32 board support
3. Install required libraries:
   - ESP32 BLE Arduino
   - Preferences (built-in)
4. Open `ESP32_Sensor_Node.ino`
5. Configure default cloud node MAC address (line 20):
   ```cpp
   uint8_t cloudNodeAddress[6] = {0x0C, 0x4E, 0xA0, 0x4D, 0x54, 0x8C};
   ```
6. Upload to ESP32

### 3. Initial Configuration
1. Power on the device
2. Connect via BLE using your frontend app
3. Send WiFi credentials via SSID and Password characteristics
4. Update device properties via Properties characteristic
5. Device will save all settings and restart

## Operation

### Normal Operating Cycle
1. **Wake from deep sleep**
2. **Load stored configuration** (WiFi, properties, gateway MAC)
3. **Connect to WiFi** using stored credentials
4. **Initialize ESP-NOW** and find gateway
5. **Read sensor** (ultrasonic distance)
6. **Calculate values** (level %, litres)
7. **Read battery voltage**
8. **Send data** to gateway via ESP-NOW
9. **Enter deep sleep** for configured refresh rate

### Power Management
- **Active time**: ~2-5 seconds per cycle
- **Sleep time**: Configurable (default 5 minutes)
- **Deep sleep current**: <10µA
- **Sensor power control**: Powered only during measurement

### Battery Monitoring
- Voltage divider ratio: 2.0 (100kΩ / 100kΩ)
- ADC resolution: 12-bit
- Voltage range: 0-3.3V input (0-6.6V battery)

## Configuration via Frontend

### Connect to Device
1. Scan for BLE devices
2. Connect to `ESP32-Sensor-Node`
3. Subscribe to Device Info and Status characteristics

### Update Properties
1. Read current properties from Device Info characteristic
2. Modify desired values
3. Write JSON to Properties characteristic
4. Wait for `properties_updated` confirmation
5. Device will update ESP-NOW peer if gateway MAC changed

### Change WiFi Network
1. Write new SSID to SSID characteristic
2. Write new password to Password characteristic
3. Device attempts connection and sends status updates

## Troubleshooting

### Device Not Advertising BLE
- **Cold boot required**: BLE only activates on power-on reset
- **Timeout expired**: BLE disables after 2 minutes (5 minutes in provisioning mode)
- **Solution**: Power cycle the device

### Cannot Find Gateway
- **Check gateway is powered and connected to WiFi**
- **Verify MAC address** is correct (visible in gateway serial output)
- **Check range**: ESP-NOW range is ~100m line-of-sight
- **Update gateway MAC**: Send new MAC via Properties characteristic

### WiFi Connection Fails
- **Check credentials**: Ensure SSID and password are correct
- **Signal strength**: Move device closer to router
- **Check 2.4GHz**: ESP32 only supports 2.4GHz WiFi
- **Clear stored credentials**: Will auto-clear and retry on next boot

### Inaccurate Tank Readings
- **Calibrate distances**: Measure actual full/empty distances
- **Update via BLE**: Send correct minDistance and maxDistance
- **Check sensor**: Ensure SR04M-2 is mounted securely
- **Verify wiring**: Test sensor power control circuit

### Battery Drains Quickly
- **Increase refresh rate**: Longer sleep = longer battery life
- **Check for deep sleep**: Verify device enters sleep (check serial)
- **Disable debugging**: Comment out Serial.print statements for production
- **Check sensor power**: Ensure transistor cuts power when not measuring

## Storage Locations (NVS)

### Namespace: "wifi"
- `ssid` - WiFi network name (String)
- `password` - WiFi password (String)

### Namespace: "device"
- `minDist` - Minimum distance in cm (Float)
- `maxDist` - Maximum distance in cm (Float)
- `refreshRate` - Refresh rate in seconds (UInt32)
- `totalLitres` - Tank capacity in litres (Float)
- `cloudMAC` - Gateway MAC address (Bytes[6])

## Sleep Configuration

Current sleep time: **5 minutes** (300 seconds)

To modify, edit `config.h`:
```cpp
static const uint64_t SLEEP_TIME_US = 5ULL * 60ULL * 1000000ULL;
```

To enable deep sleep in the code, uncomment in `ESP32_Sensor_Node.ino` line 228-229:
```cpp
esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
esp_deep_sleep_start();
```

## File Structure

```
ESP32_Sensor_Node/
├── ESP32_Sensor_Node.ino    # Main program
├── config.h                  # Configuration and constants
├── sensor.h/cpp             # Ultrasonic sensor functions
├── espnow_comm.h/cpp        # ESP-NOW communication
├── provisioning.h/cpp       # BLE WiFi provisioning
└── README.md                # This file
```

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| Operating Voltage | 3.3V |
| WiFi | 2.4GHz 802.11 b/g/n |
| BLE | Bluetooth 5.0 LE |
| ESP-NOW Range | ~100m (line-of-sight) |
| Sensor Range | 20cm - 450cm (SR04M-2) |
| Sensor Accuracy | ±1cm |
| Current (Active) | ~80mA |
| Current (Sleep) | <10µA |
| Storage | 512KB flash (NVS) |

## License

This project is open source. Modify and use as needed for your application.

## Notes

- First boot takes longer due to channel scanning
- Subsequent boots are faster using saved channel
- BLE provisioning works without WiFi connection
- All configuration persists across power cycles
- Deep sleep preserves RTC memory (channel info)
- Battery life depends on refresh rate and battery capacity

## Support

For issues or questions:
1. Check serial monitor output for detailed logs
2. Verify all hardware connections
3. Ensure ESP32 board package is up to date
4. Check frontend app implements correct BLE UUIDs
