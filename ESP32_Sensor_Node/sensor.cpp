/*
 * Ultrasonic Sensor Implementation
 */

#include "sensor.h"

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float readDistanceCm() {
  // Power on the sensor via NPN transistor
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);
  delay(50);  // Allow sensor to stabilize
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long durationUs = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (durationUs == 0) {
    return NAN;
  }

  float distance = (durationUs * speedOfSoundCmPerUs) / 2.0f;

  // SR04M-2 range: 20cm to 620cm
  if (distance < 20.0f || distance > 620.0f) {
    digitalWrite(SENSOR_POWER_PIN, LOW);  // Power off sensor
    return NAN;
  }

  digitalWrite(SENSOR_POWER_PIN, LOW);  // Power off sensor
  return distance;
}

float readSmoothedDistanceCm() {
  float vals[samplesPerUpdate];
  int good = 0;

  for (int i = 0; i < samplesPerUpdate; i++) {
    float d = readDistanceCm();
    if (!isnan(d)) {
      vals[good++] = d;
    }
    delay(120);
  }

  if (good == 0) return NAN;

  // Simple sort
  for (int i = 0; i < good - 1; i++) {
    for (int j = i + 1; j < good; j++) {
      if (vals[j] < vals[i]) {
        float t = vals[i];
        vals[i] = vals[j];
        vals[j] = t;
      }
    }
  }

  // Trim extremes if enough values
  int start = 0;
  int end = good - 1;
  if (good >= 5) {
    start = 1;
    end = good - 2;
  }

  float sum = 0;
  int count = 0;
  for (int i = start; i <= end; i++) {
    sum += vals[i];
    count++;
  }

  return sum / (float)count;
}

float readBatteryVoltage() {
  // ESP32-C3 ADC is 12-bit (0-4095) with default 3.3V reference
  // Set ADC attenuation to 11dB for 0-3.3V range
  analogSetAttenuation(ADC_11db);
  
  int adcValue = analogRead(BATTERY_VOLTAGE_PIN);
  
  Serial.print("ADC Raw Value: "); Serial.println(adcValue);
  
  // Convert ADC reading to voltage (3.3V reference, 12-bit ADC)
  float voltageAtPin = (adcValue / 4095.0f) * 3.3f;
  
  Serial.print("Voltage at Pin: "); Serial.print(voltageAtPin, 3); Serial.println("V");
  
  // Calculate actual battery voltage using voltage divider ratio
  float batteryVoltage = voltageAtPin * VOLTAGE_DIVIDER_RATIO;
  
  return batteryVoltage;
}
