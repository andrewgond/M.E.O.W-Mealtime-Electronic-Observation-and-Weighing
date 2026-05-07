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
	Motor logic lives in feeder.h (move_motor, feed_grams, setup_motor).

	Routes:
		GET  /           → serves index.html from SPIFFS
		GET  /data       → returns live sensor JSON
		GET  /feed       → closed-loop dispense (?amount=XX)
		GET  /moveMotor  → manual motor test rotation
		GET  /tare       → tares both scales
		POST /setSchedule → receives schedule JSON from web UI
*/

// ── Server instance ───────────────────────────────────────
const char* MDNS_NAME = "meow";
WebServer server(80);

// ── Dispense amount — set by web UI feed button ───────────
float dispenseAmount = 50.0;  // grams

// ── Last RFID tag seen — updated by main loop ─────────────
uint32_t lastRfidTag = 0;

// ── Web schedule struct & storage ────────────────────────
struct WebSchedule {
	String   catId;
	String   type;          // "fixed" or "interval"
	String   time;          // "HH:MM" for fixed type
	int      intervalHours; // for interval type
	int      amount;        // grams
	uint64_t lastFired;     // unix timestamp of last dispense
};
std::vector<WebSchedule> webSchedules;

// ═══════════════════════════════════════════════════════
// ROUTE HANDLERS
// ═══════════════════════════════════════════════════════

// Serves the dashboard HTML from SPIFFS (data/index.html).
void handleRoot() {
	File file = SPIFFS.open("/index.html", "r");
	if (!file) {
		server.send(404, "text/plain", "index.html not found — re-upload SPIFFS");
		return;
	}
	server.streamFile(file, "text/html");
	file.close();
}

// Returns live sensor data as JSON, polled every second by the dashboard.
// { "catWeight": X.XX, "foodWeight": XXX, "dispenseAmount": XX,
//   "rfid": XXXXXXXX, "timesynced": true, "currentTime": "HH:MM" }
void handleData() {
	float foodWeight     = get_food_weight();
	float platformWeight = get_platform_weight();
	float catWeight      = platformWeight / 1000.0;  // g → kg

	String currentTime = "--:--";
	struct tm timeinfo;
	if (getLocalTime(&timeinfo)) {
		char buf[6];
		strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
		currentTime = String(buf);
	}

	String json = "{";
	json += "\"catWeight\":"      + String(catWeight,      2) + ",";
	json += "\"foodWeight\":"     + String(foodWeight,     0) + ",";
	json += "\"dispenseAmount\":" + String(dispenseAmount, 0) + ",";
	json += "\"rfid\":"           + String(lastRfidTag)        + ",";
	json += "\"timesynced\":"     + String(is_time_synced() ? "true" : "false") + ",";
	json += "\"currentTime\":\"" + currentTime + "\"";
	json += "}";
	server.send(200, "application/json", json);
}

// Feed Now button — closed-loop dispense using feed_grams() from feeder.h
void handleFeed() {
	if (server.hasArg("amount")) {
		float req = server.arg("amount").toFloat();
		if (req > 0) dispenseAmount = req;
	}
	if (DEBUG_MODE) Serial.printf("[FEED] Manual feed: %.1fg\n", dispenseAmount);
	float actual = feed_grams(dispenseAmount);
	server.send(200, "text/plain", "Dispensed: " + String(actual, 1) + "g");
}

// Tare Scales button — zeroes both load cells.
void handleTare() {
	if (DEBUG_MODE) Serial.println("[HX711] Taring both scales");
	tare_scales();
	server.send(200, "text/plain", "Tared");
}

// Receives schedule JSON from the web UI and rebuilds webSchedules.
// Format: { "catId": [ { "type":"fixed","time":"08:00","amount":50 }, ... ], ... }
void handleSetSchedule() {
	if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
	String body = server.arg("plain");

	webSchedules.clear();

	int pos = 0;
	while ((pos = body.indexOf('"', pos)) != -1) {
		int keyStart = pos + 1;
		int keyEnd   = body.indexOf('"', keyStart);
		if (keyEnd == -1) break;
		String catId = body.substring(keyStart, keyEnd);
		pos = keyEnd + 1;

		int arrStart = body.indexOf('[', pos);
		if (arrStart == -1) break;
		int arrEnd = body.indexOf(']', arrStart);
		if (arrEnd == -1) break;
		String arr = body.substring(arrStart, arrEnd + 1);
		pos = arrEnd + 1;

		int objPos = 0;
		while ((objPos = arr.indexOf('{', objPos)) != -1) {
			int objEnd = arr.indexOf('}', objPos);
			if (objEnd == -1) break;
			String obj = arr.substring(objPos, objEnd + 1);
			objPos = objEnd + 1;

			WebSchedule s;
			s.catId     = catId;
			s.lastFired = 0;

			int ti = obj.indexOf("\"type\"");
			if (ti != -1) { int vs = obj.indexOf('"', ti+7)+1; s.type = obj.substring(vs, obj.indexOf('"', vs)); }

			int tmi = obj.indexOf("\"time\"");
			if (tmi != -1) { int vs = obj.indexOf('"', tmi+7)+1; s.time = obj.substring(vs, obj.indexOf('"', vs)); }

			int ii = obj.indexOf("\"intervalHours\"");
			if (ii != -1) { s.intervalHours = obj.substring(obj.indexOf(':', ii)+1).toInt(); }

			int ai = obj.indexOf("\"amount\"");
			if (ai != -1) { s.amount = obj.substring(obj.indexOf(':', ai)+1).toInt(); }

			if (s.amount > 0) webSchedules.push_back(s);
		}
	}

	if (DEBUG_MODE) Serial.printf("[SCHED] Loaded %d schedules\n", webSchedules.size());
	server.send(200, "text/plain", "OK");
}

// ═══════════════════════════════════════════════════════
// SCHEDULED FEEDING (web UI schedules)
// Checks webSchedules and fires any that are due.
// Call from loop() every iteration.
// ═══════════════════════════════════════════════════════
void handle_scheduled_feeding() {
	if (!is_time_synced() || webSchedules.empty()) return;

	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) return;
	uint64_t now = get_time();

	for (auto& s : webSchedules) {
		if (s.type == "fixed" && s.time.length() == 5) {
			int schedH = s.time.substring(0, 2).toInt();
			int schedM = s.time.substring(3, 5).toInt();
			if (timeinfo.tm_hour == schedH && timeinfo.tm_min == schedM && (now - s.lastFired) > 60) {
				if (DEBUG_MODE) Serial.printf("[SCHED] Fixed: %s at %s — %dg\n", s.catId.c_str(), s.time.c_str(), s.amount);
				feed_grams(s.amount);
				s.lastFired = now;
			}
		} else if (s.type == "interval" && s.intervalHours > 0) {
			uint64_t intervalSec = (uint64_t)s.intervalHours * 3600;
			if (s.lastFired == 0) {
				s.lastFired = now;
			} else if (now - s.lastFired >= intervalSec) {
				if (DEBUG_MODE) Serial.printf("[SCHED] Interval: %s every %dh — %dg\n", s.catId.c_str(), s.intervalHours, s.amount);
				feed_grams(s.amount);
				s.lastFired = now;
			}
		}
	}
}

// Returns database entries as JSON array for the History page.
// GET /getEntries?start=0&end=9999999999&cat=0
// cat=0 means all cats
void handleGetEntries() {
	uint64_t start  = server.hasArg("start") ? (uint64_t)server.arg("start").toInt() : 0;
	uint64_t end    = server.hasArg("end")   ? (uint64_t)server.arg("end").toInt()   : get_time();
	uint32_t cat_id = server.hasArg("cat")   ? (uint32_t)server.arg("cat").toInt()   : 0;

	// Collect entries into a vector first (query_entry_range needs plain fn pointer, no lambdas)
	static std::vector<Entry> results;
	results.clear();

	query_entry_range(start, end, cat_id, [](Entry e) {
		results.push_back(e);
	});

	// Build JSON from collected results
	String json = "[";
	for (int i = 0; i < (int)results.size(); i++) {
		if (i > 0) json += ",";
		const Entry& e = results[i];
		json += "{";
		json += "\"time\":"            + String((uint32_t)e.time)        + ",";
		json += "\"assumed_cat_id\":"  + String(e.assumed_cat_id)        + ",";
		json += "\"rfid\":"            + String(e.rfid)                  + ",";
		json += "\"food_weight\":"     + String(e.food_weight,    2)     + ",";
		json += "\"platform_weight\":" + String(e.platform_weight, 2);
		json += "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════
// CAT PROFILE ENDPOINTS
// ═══════════════════════════════════════════════════════

// Returns all cat profiles as JSON array.
// GET /getCats
// [ { "id": 1, "rfid": 12345, "weight": 4.5, "name": "Luna" }, ... ]
void handleGetCats() {
	String json = "[";
	for (int i = 0; i < (int)cat_profiles.size(); i++) {
		if (i > 0) json += ",";
		json += "{";
		json += "\"id\":"     + String(cat_profiles[i].id)     + ",";
		json += "\"rfid\":"   + String(cat_profiles[i].rfid)   + ",";
		json += "\"weight\":" + String(cat_profiles[i].weight, 2) + ",";
		json += "\"name\":\"" + String(cat_profiles[i].name)   + "\"";
		json += "}";
	}
	json += "]";
	server.send(200, "application/json", json);
}

// Adds a new cat profile to the database.
// POST /addCat  body: name=Luna&rfid=12345&weight=4.5
void handleAddCat() {
	if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }

	const char* name  = server.arg("name").c_str();
	uint32_t    rfid  = server.hasArg("rfid")   ? (uint32_t)server.arg("rfid").toInt()     : 0;
	float       weight = server.hasArg("weight") ? server.arg("weight").toFloat()           : 0.0f;

	int new_id = add_cat(rfid, weight, name);
	if (new_id > 0) {
		if (DEBUG_MODE) Serial.printf("[CAT] Added: %s (id=%d rfid=%u weight=%.2f)\n", name, new_id, rfid, weight);
		server.send(200, "application/json",
			"{\"id\":" + String(new_id) + ",\"name\":\"" + String(name) + "\"}");
	} else {
		server.send(500, "text/plain", "Failed to add cat — check SD card connection");
	}
}

// Removes a cat profile by ID.
// GET /removeCat?id=1
void handleRemoveCat() {
	if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing id"); return; }
	uint32_t id = (uint32_t)server.arg("id").toInt();
	if (remove_cat(id)) {
		if (DEBUG_MODE) Serial.printf("[CAT] Removed id=%u\n", id);
		server.send(200, "text/plain", "Removed");
	} else {
		server.send(500, "text/plain", "Failed to remove cat");
	}
}

// ═══════════════════════════════════════════════════════
// SERVER SETUP — call from main setup() after WiFi connects
// ═══════════════════════════════════════════════════════
void setup_website() {
	if (!SPIFFS.begin(true)) {
		if (DEBUG_MODE) Serial.println("[SPIFFS] Failed — dashboard won't load");
	} else {
		if (DEBUG_MODE) Serial.println("[SPIFFS] Ready");
	}

	server.on("/",            handleRoot);
	server.on("/data",        handleData);
	server.on("/feed",        handleFeed);
	server.on("/tare",        handleTare);
	server.on("/setSchedule", HTTP_POST, handleSetSchedule);
	server.on("/getCats",     handleGetCats);
	server.on("/addCat",      HTTP_POST, handleAddCat);
	server.on("/removeCat",   handleRemoveCat);
	server.on("/getEntries",  handleGetEntries);

	server.begin();
	if (DEBUG_MODE) Serial.println("[SERVER] Ready — http://meow.local");
}