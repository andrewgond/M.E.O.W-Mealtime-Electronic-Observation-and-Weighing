#pragma once
#include "config.h"
#include "network.h"
#include "scales.h"
#include "feeder.h"
#include "database.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include "SPIFFS.h"

/*
	website.h — M.E.O.W Web Server

	Routes:
		GET  /             → serves index.html from SPIFFS
		GET  /data         → returns live sensor JSON
		POST /feed         → closed-loop dispense (?amount=XX)
		GET  /tare         → tares both scales
		GET  /getEntries   → DB entries (?start&end&cat)
		GET  /getCats      → all cat profiles as JSON
		POST /addCat       → add cat (name, rfid, weight)
		GET  /removeCat    → remove cat (?id)
		GET  /getSchedules → all schedules as JSON
		POST /addSchedule  → add schedule (base_time, grams)
		GET  /removeSchedule → remove schedule (?id)

	Scheduling is handled entirely by database.h (handle_schedules).
	Schedules persist across reboots via SQLite.
*/

// ── Server instance ───────────────────────────────────────
const char* MDNS_NAME = "meow";
WebServer server(80);

// ── Dispense amount — set by web UI feed button ───────────
float dispenseAmount = 50.0;

// ── Last RFID tag seen — updated by main loop ─────────────
uint32_t lastRfidTag = 0;

// Called from feeder.h during dispensing to keep web server alive
void website_handle_client() {
	server.handleClient();
}

// ═══════════════════════════════════════════════════════
// ROUTE HANDLERS
// ═══════════════════════════════════════════════════════

void handleRoot() {
	File file = SPIFFS.open("/index.html", "r");
	if (!file) { server.send(404, "text/plain", "index.html not found"); return; }
	server.streamFile(file, "text/html");
	file.close();
}

// Live sensor JSON — polled every second by dashboard
void handleData() {
	float foodWeight     = get_food_weight();
	float platformWeight = get_platform_weight();
	float catWeight      = platformWeight / 1000.0;

	String currentTime = "--:--";
	struct tm timeinfo;
	if (getLocalTime(&timeinfo)) {
		char buf[6];
		strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
		currentTime = String(buf);
	}

	String json = "{";
	json += "\"catWeight\":"       + String(catWeight,      2) + ",";
	json += "\"foodWeight\":"      + String(foodWeight,     0) + ",";
	json += "\"dispenseAmount\":"  + String(dispenseAmount, 0) + ",";
	json += "\"rfid\":"            + String(lastRfidTag)        + ",";
	json += "\"timesynced\":"      + String(is_time_synced()   ? "true" : "false") + ",";
	json += "\"currentTime\":\""   + currentTime + "\",";
	json += "\"wifiConnected\":"   + String(is_wifi_connected() ? "true" : "false") + ",";
	json += "\"dbConnected\":"     + String(is_db_connected()   ? "true" : "false") + ",";
	json += "\"dbWriteErr\":"      + String(write_err            ? "true" : "false");
	json += "}";
	server.send(200, "application/json", json);
}

// Feed Now — closed-loop dispense
void handleFeed() {
	if (server.hasArg("amount")) {
		float req = server.arg("amount").toFloat();
		if (req > 0) dispenseAmount = req;
	}
	if (DEBUG_MODE) Serial.printf("[FEED] Manual: %.1fg\n", dispenseAmount);
	float actual = feed_grams(dispenseAmount);
	server.send(200, "text/plain", "Dispensed: " + String(actual, 1) + "g");
}

// Tare both scales
void handleTare() {
	if (DEBUG_MODE) Serial.println("[HX711] Taring");
	tare_scales();
	server.send(200, "text/plain", "Tared");
}

// ═══════════════════════════════════════════════════════
// SCHEDULE ENDPOINTS (backed by Craig's database.h)
// ═══════════════════════════════════════════════════════

// GET /getSchedules
// Returns all schedules as JSON: [ { "id", "base_time", "grams" }, ... ]
void handleGetSchedules() {
	String json = "[";
	for (int i = 0; i < (int)schedules.size(); i++) {
		if (i > 0) json += ",";
		json += "{";
		json += "\"id\":"        + String(schedules[i].id)        + ",";
		json += "\"base_time\":" + String((uint32_t)schedules[i].base_time) + ",";
		json += "\"grams\":"     + String(schedules[i].grams, 1);
		json += "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

// POST /addSchedule  body: base_time=SECONDS_SINCE_MIDNIGHT&grams=50
// base_time = seconds since midnight (e.g. 8:00 AM = 28800)
void handleAddSchedule() {
	if (!server.hasArg("base_time") || !server.hasArg("grams")) {
		server.send(400, "text/plain", "Missing base_time or grams");
		return;
	}
	uint64_t base_time = (uint64_t)server.arg("base_time").toInt();
	float    grams     = server.arg("grams").toFloat();

	int new_id = add_schedule(base_time, grams);
	if (new_id > 0) {
		if (DEBUG_MODE) Serial.printf("[SCHED] Added id=%d base=%llu grams=%.1f\n", new_id, base_time, grams);
		server.send(200, "application/json",
			"{\"id\":" + String(new_id) + ",\"base_time\":" + String((uint32_t)base_time) + ",\"grams\":" + String(grams,1) + "}");
	} else {
		server.send(500, "text/plain", "Failed — check SD card");
	}
}

// GET /removeSchedule?id=1
void handleRemoveSchedule() {
	if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing id"); return; }
	uint32_t id = (uint32_t)server.arg("id").toInt();
	if (remove_schedule(id)) {
		if (DEBUG_MODE) Serial.printf("[SCHED] Removed id=%u\n", id);
		server.send(200, "text/plain", "Removed");
	} else {
		server.send(500, "text/plain", "Failed to remove schedule");
	}
}

// ═══════════════════════════════════════════════════════
// DB ENTRY ENDPOINTS
// ═══════════════════════════════════════════════════════

// GET /getEntries?start=0&end=9999999999&cat=0
// Returns most recent 200 entries (ordered by DB)
void handleGetEntries() {
	uint64_t start  = server.hasArg("start") ? (uint64_t)server.arg("start").toInt() : 0;
	uint64_t end    = server.hasArg("end")   ? (uint64_t)server.arg("end").toInt()   : get_time();
	uint32_t cat_id = server.hasArg("cat")   ? (uint32_t)server.arg("cat").toInt()   : 0;

	static std::vector<Entry> results;
	results.clear();
	query_entry_range(start, end, cat_id, [](Entry e) { results.push_back(e); });

	String json = "[";
	for (int i = 0; i < (int)results.size(); i++) {
		if (i > 0) json += ",";
		const Entry& e = results[i];
		json += "{";
		json += "\"time\":"            + String((uint32_t)e.time)      + ",";
		json += "\"assumed_cat_id\":"  + String(e.assumed_cat_id)      + ",";
		json += "\"rfid\":"            + String(e.rfid)                + ",";
		json += "\"food_weight\":"     + String(e.food_weight,    2)   + ",";
		json += "\"platform_weight\":" + String(e.platform_weight, 2);
		json += "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════
// CAT PROFILE ENDPOINTS
// ═══════════════════════════════════════════════════════

// GET /getCats
void handleGetCats() {
	String json = "[";
	for (int i = 0; i < (int)cat_profiles.size(); i++) {
		if (i > 0) json += ",";
		json += "{";
		json += "\"id\":"     + String(cat_profiles[i].id)        + ",";
		json += "\"rfid\":"   + String(cat_profiles[i].rfid)      + ",";
		json += "\"weight\":" + String(cat_profiles[i].weight, 2) + ",";
		json += "\"name\":\"" + String(cat_profiles[i].name)      + "\"";
		json += "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

// POST /addCat  body: name=Luna&rfid=12345&weight=4.5
void handleAddCat() {
	if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
	const char* name   = server.arg("name").c_str();
	uint32_t    rfid   = server.hasArg("rfid")   ? (uint32_t)server.arg("rfid").toInt() : 0;
	float       weight = server.hasArg("weight") ? server.arg("weight").toFloat()        : 0.0f;

	int new_id = add_cat(rfid, weight, name);
	if (new_id > 0) {
		server.send(200, "application/json",
			"{\"id\":" + String(new_id) + ",\"name\":\"" + String(name) + "\"}");
	} else {
		server.send(500, "text/plain", "Failed — check SD card");
	}
}

// GET /removeCat?id=1
void handleRemoveCat() {
	if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing id"); return; }
	uint32_t id = (uint32_t)server.arg("id").toInt();
	bool ok = remove_cat(id);
	server.send(ok ? 200 : 500, "text/plain", ok ? "Removed" : "Failed");
}

// ═══════════════════════════════════════════════════════
// SERVER SETUP
// ═══════════════════════════════════════════════════════
void setup_website() {
	if (!SPIFFS.begin(true)) {
		if (DEBUG_MODE) Serial.println("[SPIFFS] Failed");
	} else {
		if (DEBUG_MODE) Serial.println("[SPIFFS] Ready");
	}

	server.on("/",               handleRoot);
	server.on("/data",           handleData);
	server.on("/feed",           handleFeed);
	server.on("/tare",           handleTare);
	server.on("/getEntries",     handleGetEntries);
	server.on("/getCats",        handleGetCats);
	server.on("/addCat",         HTTP_POST, handleAddCat);
	server.on("/removeCat",      handleRemoveCat);
	server.on("/getSchedules",   handleGetSchedules);
	server.on("/addSchedule",    HTTP_POST, handleAddSchedule);
	server.on("/removeSchedule", handleRemoveSchedule);

	server.begin();
	if (DEBUG_MODE) Serial.println("[SERVER] Ready — http://meow.local");
}