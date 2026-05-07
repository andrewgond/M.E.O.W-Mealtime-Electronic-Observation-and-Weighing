#include "config.h"
#include "network.h"
#include "database.h"
#include "rfid.h"
#include "scales.h"
#include "website.h"

SPIClass sd_spi = SPIClass(VSPI);
HardwareSerial rfid_serial(2);

// debug for viewing an entry
inline void print_entry(Entry entry) {
	Serial.printf(
		"[FOOD: %7.2fg] "
		"[PLATFORM: %7.2fg] "
		"[RFID: %010u] "
		"= [ID: %03u] "
		"@ %s",
		entry.food_weight,
		entry.platform_weight,
		entry.rfid,
		entry.assumed_cat_id,
		entry.time == 0 ? "UNKNOWN\n" : ctime((time_t*)&entry.time)
	);
}

void setup() {
	if (DEBUG_MODE) Serial.begin(115200);
	if (DEBUG_MODE) Serial.println("Setup begin...");
	delay(1000); // delay for safety

	// init with pins
	sd_spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
	rfid_serial.begin(9600, SERIAL_8N1, PIN_RFID_RX, PIN_RFID_TX);
	scale_food.begin(PIN_SCALE_FOOD_DOUT, PIN_SCALE_FOOD_SCK);
	scale_platform.begin(PIN_SCALE_PLATFORM_DOUT, PIN_SCALE_PLATFORM_SCK);

	// setup motor
	pinMode(PIN_MOTOR_DIR,  OUTPUT);
	pinMode(PIN_MOTOR_STEP, OUTPUT);
	digitalWrite(PIN_MOTOR_DIR,  HIGH);
	digitalWrite(PIN_MOTOR_STEP, LOW);

	// scale setup
	scale_food.set_scale(CALIBRATION_FOOD);
	scale_platform.set_scale(CALIBRATION_PLATFORM);
	tare_scales();

	// connect network
	if (connect_wifi()) {
		sync_time();
		MDNS.begin(MDNS_NAME);
	}

	// website init
	setup_website();

	// connect database
	connect_db(sd_spi, PIN_SD_CS);

	if (DEBUG_MODE) Serial.println("Setup end.");
}

void loop() {
	// connect with wifi
	static unsigned long last_wifi_attempt = 0;
	constexpr unsigned long INTERVAL_WIFI_ATTEMPT = 10000; // 10s interval
	if (!is_wifi_connected() && millis() - last_wifi_attempt > INTERVAL_WIFI_ATTEMPT) {
		last_wifi_attempt = millis();
		connect_wifi();
		if (is_wifi_connected()) MDNS.begin(MDNS_NAME);
	}

	// sync time
	static unsigned long last_time_attempt = 0;
	constexpr unsigned long INTERVAL_TIME_ATTEMPT = 10000; // 10s interval
	if (!is_time_synced() && is_wifi_connected() && millis() - last_time_attempt > INTERVAL_TIME_ATTEMPT) {
		last_time_attempt = millis();
		sync_time();
	}

	// connect database
	static unsigned long last_db_attempt = 0;
	constexpr unsigned long INTERVAL_DB_ATTEMPT = 10000; // 10s interval
	if (!is_db_connected() && millis() - last_db_attempt > INTERVAL_DB_ATTEMPT) {
		last_db_attempt = millis();
		connect_db(sd_spi, PIN_SD_CS);
	}

	// updates scale readings
	tick_scales();

	// handle scheduled feedings
	handle_schedules();

	// gather current data
	static Entry current_entry;
	current_entry.rfid = get_rfid(rfid_serial);
	current_entry.food_weight = get_food_weight();
	current_entry.platform_weight = get_platform_weight();

	// select the best data from the current entries
	static Entry best_representation = {0};
	static float food_range = std::numeric_limits<float>::infinity();
	static float platform_range = std::numeric_limits<float>::infinity();
	if (current_entry.rfid != 0) best_representation.rfid = current_entry.rfid;

	constexpr float STABILITY_THRESHOLD_FOOD = 0.5;
	float new_food_range = get_food_stability();
	if (new_food_range <= STABILITY_THRESHOLD_FOOD) {
		// stable, keep lightest or newer if prev was unstable
		if (food_range > STABILITY_THRESHOLD_FOOD || current_entry.food_weight < best_representation.food_weight) {
			food_range = new_food_range;
			best_representation.food_weight = current_entry.food_weight;
		}
	} else if (new_food_range <= food_range) {
		// unstable, use the most stable
		food_range = new_food_range;
		best_representation.food_weight = current_entry.food_weight;
	}

	constexpr float STABILITY_THRESHOLD_PLATFORM = 0.5;
	float new_platform_range = get_platform_stability();
	if (new_platform_range <= STABILITY_THRESHOLD_PLATFORM) {
		// stable, keep heaviest or newer if prev was unstable
		if (platform_range > STABILITY_THRESHOLD_PLATFORM || current_entry.platform_weight > best_representation.platform_weight) {
			platform_range = new_platform_range;
			best_representation.platform_weight = current_entry.platform_weight;
		}
	} else if (new_platform_range <= platform_range) {
		// unstable, use the most stable
		platform_range = new_platform_range;
		best_representation.platform_weight = current_entry.platform_weight;
	}

	// keep track if it's okay to use the previous rfid scan
	static bool rfid_scanned_this_session = false;
	if (current_entry.platform_weight == 0) rfid_scanned_this_session = false;

	// push best representation to the db every second
	static unsigned long last_push = 0;
	constexpr unsigned long INTERVAL_PUSH = 1000; // 1s interval
	if (is_time_synced() && millis() - last_push > INTERVAL_PUSH) {
		last_push = millis();
		best_representation.time = get_time();

		// determine the cat if on platform
		static uint32_t active_cat = 0;
		if (best_representation.platform_weight != 0) {
			if (best_representation.rfid != 0) {
				rfid_scanned_this_session = true;
				active_cat = map_rfid_to_cat(best_representation.rfid);
			} else {
				if (!rfid_scanned_this_session) {
					active_cat = map_weight_to_cat(best_representation.platform_weight);
				}
			}
			best_representation.assumed_cat_id = active_cat;
			if (best_representation.rfid != 0) lastRfidTag = best_representation.rfid;
		} else {
			best_representation.assumed_cat_id = 0;
		}

		// always push entry to db (food weight logged even without cat)
		if (is_db_connected()) {
			if (DEBUG_MODE) print_entry(best_representation);
			push_entry(best_representation);
		}

		// reset best rep
		best_representation = {0};
		food_range = std::numeric_limits<float>::infinity();
		platform_range = std::numeric_limits<float>::infinity();
	}

	// keep web server alive
	server.handleClient();
}