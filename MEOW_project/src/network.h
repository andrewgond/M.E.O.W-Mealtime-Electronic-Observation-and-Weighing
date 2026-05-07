#pragma once
#include <WiFi.h>
#include "esp_sntp.h"

// wifi
const char* WIFI_SSID		= "ESP";
const char* WIFI_PASSWORD	= "11223344";
// const char* WIFI_SSID		= "ESP";
// const char* WIFI_PASSWORD	= "11223344";

// time (PDT)
const char* TIME_NTP_SERVER	= "pool.ntp.org";
constexpr long gmtOffset_sec = -8 * 60 * 60;
constexpr int daylightOffset_sec = 1 * 60 * 60;

bool synced = false;

inline bool is_wifi_connected() {
	return WiFi.status() == WL_CONNECTED;
}

bool connect_wifi() {
	if (DEBUG_MODE) Serial.println("Connecting to WiFi...");
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	
	// wait for connection
	auto start = millis();
	constexpr unsigned long TIMEOUT = 10000; // 10s timeout
	while (millis() - start < TIMEOUT) {
		auto status = WiFi.status();
		if (status == WL_CONNECTED) {
			if (DEBUG_MODE) Serial.println("WiFi connected.");
			return true;
		}
		if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
			if (DEBUG_MODE) Serial.println("WiFi connection failure.");
			return false;
		}
		delay(100);
	}

	// wifi timeout
	if (DEBUG_MODE) Serial.println("WiFi connection timeout.");
	return false;
}

inline bool is_time_synced() {
	return synced;
}

bool sync_time() {
	if (DEBUG_MODE) Serial.println("Syncing time...");
	configTime(gmtOffset_sec, daylightOffset_sec, TIME_NTP_SERVER);
	constexpr unsigned long TIMEOUT = 10000; // 10s timeout
	tm info;
	synced = getLocalTime(&info, TIMEOUT);
	if (DEBUG_MODE) Serial.println(synced ? "Time synced." : "Time sync failed.");
	return synced;
}

inline uint64_t get_time() {
	return (uint64_t)time(nullptr);
}