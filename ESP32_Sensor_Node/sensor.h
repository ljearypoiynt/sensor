/*
 * Ultrasonic Sensor Functions
 * 
 * Handles reading from the SR04M-2 ultrasonic sensor
 */

#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "config.h"

// Returns distance in cm, or NAN if timeout / invalid
// Standard trigger/echo mode - works with SR04M-2 RX/TX pins
float readDistanceCm();

// Take multiple samples and return median-ish (trimmed mean)
float readSmoothedDistanceCm();

// Read battery voltage from voltage divider on ADC pin
float readBatteryVoltage();

// Helper function to clamp values
static float clampf(float v, float lo, float hi);

#endif // SENSOR_H
