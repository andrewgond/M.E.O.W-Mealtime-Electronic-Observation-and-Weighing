#pragma once
#include <stdint.h>

#define DEBUG_MODE true

// sd card
constexpr int PIN_SD_CS = 13;
constexpr int PIN_SD_MOSI = 14;
constexpr int PIN_SD_MISO = 33;
constexpr int PIN_SD_SCK = 27;

// rfid
constexpr int PIN_RFID_RX = 32;
constexpr int PIN_RFID_TX = 26;

// scales
constexpr int PIN_SCALE_FOOD_DOUT = 23;
constexpr int PIN_SCALE_FOOD_SCK = 22;
constexpr int PIN_SCALE_PLATFORM_DOUT = 19;
constexpr int PIN_SCALE_PLATFORM_SCK = 18;

constexpr float CALIBRATION_FOOD = -1043.97115385;
constexpr float CALIBRATION_PLATFORM = -23.9055944056;

// motors
constexpr int PIN_MOTOR_DIR  = 25;
constexpr int PIN_MOTOR_STEP = 26;
