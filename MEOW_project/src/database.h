#pragma once
#include <sqlite3.h>
#include <SD.h>
#include <vector>

// Forward declaration — feed_grams defined in feeder.h
float feed_grams(float targetGrams);

const char* DB_FILE = "/sd/test.db";
#define DATA_TABLE "data_table"
#define PROFILE_TABLE "cat_profiles"
#define SCHEDULE_TABLE "schedules"

struct Entry { // 24B
	uint64_t time;
	uint32_t assumed_cat_id;
	uint32_t rfid;
	float food_weight;
	float platform_weight;
};

#define ENTRY_BUF_SIZE 6 // flushes every ~6 seconds
std::array<Entry, ENTRY_BUF_SIZE> entry_buf;
int entry_buf_size = 0;

struct CatProfile {
	uint32_t id;
	uint32_t rfid;
	float weight;
	char name[32];
};

std::vector<CatProfile> cat_profiles;
bool cat_profiles_loaded = false;

constexpr uint32_t INTERVAL_24H = 24 * 60 * 60;

struct RepeatingSchedule {
	uint32_t id;
	uint64_t base_time;
	float grams;
};

std::vector<RepeatingSchedule> schedules;
uint64_t interval_num;
int schedule_idx = 0;
bool schedules_loaded = false;

bool connected = false;
bool write_err = false;

inline bool is_db_connected() {
	return connected;
}

void print_loaded_cats() {
	if (!DEBUG_MODE) return;
	if (cat_profiles.empty()) {
		Serial.println("No cats loaded.");
		return;
	}
	Serial.println("Cats loaded:");
	for (const auto& cat : cat_profiles) {
		Serial.printf("ID: %u | RFID: %u | Weight: %.2f | Name: %s\n", cat.id, cat.rfid, cat.weight, cat.name);
	}
}

bool load_cat_profiles() {
	if (DEBUG_MODE) Serial.println("Loading cat profiles...");
	sqlite3 *db;
	sqlite3_stmt *res;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return false;
	}

	const char *sql = "SELECT id, rfid, target_weight, name FROM " PROFILE_TABLE ";";
	if (sqlite3_prepare_v2(db, sql, -1, &res, NULL) == SQLITE_OK) {
		cat_profiles.clear();
		while (sqlite3_step(res) == SQLITE_ROW) {
			CatProfile p;
			p.id = (uint32_t)sqlite3_column_int(res, 0);
			p.rfid = (uint32_t)sqlite3_column_int(res, 1);
			p.weight = (uint32_t)sqlite3_column_double(res, 2);
			
			const char* name = (const char*)sqlite3_column_text(res, 3);
			if (name) {
				snprintf(p.name, sizeof(p.name), "%s", name);
			} else {
				p.name[0] = '\0';
			}
			cat_profiles.push_back(p);
		}
		sqlite3_finalize(res);
	} else {
		if (DEBUG_MODE) Serial.printf("Failed to load profiles: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_close(db);

	if (DEBUG_MODE) Serial.println("Cat profiles loaded.");
	cat_profiles_loaded = true;
	return true;
}

bool load_schedules() {
	if (!is_time_synced()) return false; // only load when time is synced
	if (DEBUG_MODE) Serial.println("Loading schedules...");
	sqlite3 *db;
	sqlite3_stmt *res;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return false;
	}

	const char *sql = "SELECT id, base_time, grams FROM " SCHEDULE_TABLE " ORDER BY base_time ASC;";
	if (sqlite3_prepare_v2(db, sql, -1, &res, NULL) == SQLITE_OK) {
		schedules.clear();
		while (sqlite3_step(res) == SQLITE_ROW) {
			RepeatingSchedule sched;
			sched.id = (uint32_t)sqlite3_column_int(res, 0);
			sched.base_time = (uint64_t)sqlite3_column_int64(res, 1);
			sched.grams = (float)sqlite3_column_double(res, 2);
			schedules.push_back(sched);
		}
		sqlite3_finalize(res);
	} else {
		if (DEBUG_MODE) Serial.printf("Failed to load schedules: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_close(db);

	if (!schedules.empty()) {
		struct tm timeinfo;
		uint64_t now = get_time();
		uint64_t today = now / INTERVAL_24H;

		// Default: mark today as done so already-passed schedules don't fire on boot
		interval_num = today;
		schedule_idx = 0;

		if (getLocalTime(&timeinfo)) {
			uint64_t seconds_today = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;

			// Find next schedule that hasn't passed yet today
			bool found = false;
			for (int i = 0; i < (int)schedules.size(); ++i) {
				if (seconds_today < schedules[i].base_time) {
					schedule_idx = i;
					interval_num = today - 1; // allow firing today for future schedules
					found = true;
					break;
				}
			}
			if (!found) {
				// All schedules already passed today — fire from beginning tomorrow
				schedule_idx = 0;
				interval_num = today;
			}
		}
	}

	if (DEBUG_MODE) Serial.println("Schedules loaded.");
	schedules_loaded = true;
	return true;
}

void connect_db(SPIClass &spi, int cs_pin) {
	if (DEBUG_MODE) Serial.println("Connecting to Database (and SD)...");
	sqlite3 *db;

	// connect to SD card
	constexpr int FREQ = 4000000; // 4MHz
	SD.end();
	if (!SD.begin(cs_pin, spi, FREQ)) {
		if (DEBUG_MODE) Serial.println("SD connection failed.");
		connected = false;
		return;
	}

	// open DB
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		if (DEBUG_MODE) Serial.println("Database open failed.");
		sqlite3_close(db);
		connected = false;
		return;
	}

	// initialize data table
	const char *sql_data =	"CREATE TABLE IF NOT EXISTS " DATA_TABLE " ("
								"time INTEGER PRIMARY KEY, "
								"assumed_cat_id INTEGER, "
								"rfid INTEGER, "
								"food_weight REAL, "
								"platform_weight REAL);";
	const char *sql_index = "CREATE INDEX IF NOT EXISTS idx_cat_time ON " DATA_TABLE " (assumed_cat_id, time);"; // TODO
	const char *sql_profiles =	"CREATE TABLE IF NOT EXISTS " PROFILE_TABLE " ("
								"id INTEGER PRIMARY KEY AUTOINCREMENT, "
								"rfid INTEGER UNIQUE, "
								"target_weight REAL, "
								"name TEXT);";
	const char *sql_schedules = "CREATE TABLE IF NOT EXISTS " SCHEDULE_TABLE " ("
								"id INTEGER PRIMARY KEY AUTOINCREMENT, "
								"base_time INTEGER, "
								"grams REAL);";
	char* zErrMsg = 0;
	if (sqlite3_exec(db, sql_data, NULL, 0, &zErrMsg) != SQLITE_OK ||
		sqlite3_exec(db, sql_index, NULL, 0, &zErrMsg) != SQLITE_OK ||
		sqlite3_exec(db, sql_profiles, NULL, 0, &zErrMsg) != SQLITE_OK ||
		sqlite3_exec(db, sql_schedules, NULL, 0, &zErrMsg) != SQLITE_OK
	) {
		if (DEBUG_MODE) Serial.println("Table initialization failed.");
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		connected = false;
		return;
	}

	// database initialized
	sqlite3_close(db);
	if (DEBUG_MODE) Serial.println("Database connected.");
	connected = true;

	// load in data
	if (!cat_profiles_loaded) load_cat_profiles();
	if (!schedules_loaded) load_schedules();
}

bool flush_buffer() {
	if (entry_buf_size == 0) return true;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	char* zErrMsg = 0;
	if (DEBUG_MODE) Serial.println("Flushing buffer...");

	// open file
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		if (DEBUG_MODE) Serial.println("Failed to open database.");
		sqlite3_close(db);
		connected = false;
		return false;
	}

	// begin insert
	write_err = false;
	sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	const char *sql_insert = "INSERT OR REPLACE INTO " DATA_TABLE " VALUES (?, ?, ?, ?, ?);";
	if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) == SQLITE_OK) {
		for (int i = 0; i < entry_buf_size; ++i) {
			sqlite3_bind_int64(stmt, 1, entry_buf[i].time);
			sqlite3_bind_int(stmt, 2, entry_buf[i].assumed_cat_id);
			sqlite3_bind_int(stmt, 3, entry_buf[i].rfid);
			sqlite3_bind_double(stmt, 4, entry_buf[i].food_weight);
			sqlite3_bind_double(stmt, 5, entry_buf[i].platform_weight);
			
			if (sqlite3_step(stmt) != SQLITE_DONE) {
				if (DEBUG_MODE) Serial.println("Database failed to write.");
				write_err = true;
				break; // begin rollback
			}
			sqlite3_reset(stmt);
		}

		// handle write failures
		if (write_err) {
			sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
			connected = false;
		} else {
			sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
			entry_buf_size = 0;
		}
		sqlite3_finalize(stmt);
	} else {
		write_err = true;
	}
	sqlite3_close(db);
	return write_err;
}

bool push_entry(Entry entry) {
	if (entry_buf_size == ENTRY_BUF_SIZE) {
		if (!is_db_connected()) return false;
		if (!flush_buffer()) return false;
		entry_buf_size = 0;
	}
	entry_buf[entry_buf_size++] = entry;
	return true;
}

// use cat_id == 0 for all cats
void query_entry_range(uint64_t start, uint64_t end, uint32_t cat_id, void (*callback)(Entry)) {
	if (DEBUG_MODE) Serial.println("Querying db now...");
	sqlite3 *db;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return;
	}

	const char *sql =	"SELECT * FROM " DATA_TABLE " "
						"WHERE (time BETWEEN ? AND ?) "
						"AND (? = 0 OR assumed_cat_id = ?) "
						"ORDER BY time DESC "
						"LIMIT 200;";
	sqlite3_stmt *res;
	if (sqlite3_prepare_v2(db, sql, -1, &res, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(res, 1, start);
		sqlite3_bind_int64(res, 2, end);
		sqlite3_bind_int(res, 3, cat_id);
		sqlite3_bind_int(res, 4, cat_id);

		while (sqlite3_step(res) == SQLITE_ROW) {
			Entry entry = {
				.time =				(uint64_t)sqlite3_column_int64(res, 0),
				.assumed_cat_id =	(uint32_t)sqlite3_column_int(res, 1),
				.rfid =				(uint32_t)sqlite3_column_int(res, 2),
				.food_weight =		(float)sqlite3_column_double(res, 3),
				.platform_weight =	(float)sqlite3_column_double(res, 4),
			};
			if (callback) callback(entry);
		}
		sqlite3_finalize(res);
	} else {
		if (DEBUG_MODE) Serial.printf("SQL Error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_close(db);
}

// returns cat id, or 0 if failed
// name must be <32
int add_cat(uint32_t rfid, float weight, const char* name) {
	if (!connected) return 0;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return 0;
	}

	uint32_t new_id = 0;
	const char* sql = "INSERT INTO " PROFILE_TABLE " (rfid, target_weight, name) VALUES (?, ?, ?);";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, rfid);
		sqlite3_bind_double(stmt, 2, weight);
		sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

		if (sqlite3_step(stmt) == SQLITE_DONE) {
			new_id = (uint32_t)sqlite3_last_insert_rowid(db);

			CatProfile p;
			p.id = new_id;
			p.rfid = rfid;
			p.weight = weight;
			snprintf(p.name, sizeof(p.name), "%s", name);

			cat_profiles.push_back(p);
			if (DEBUG_MODE) Serial.println("Cat added.");
		} else {
			if (DEBUG_MODE) Serial.println("Failed to add cat.");
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return new_id;
}

bool remove_cat(uint32_t id) {
	if (!connected) return false;
	sqlite3 *db;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return false;
	}

	// remove from db
	sqlite3_stmt *stmt;
	const char* sql = "DELETE FROM " PROFILE_TABLE " WHERE id = ?;";
	bool db_success = false;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, id);
		if (sqlite3_step(stmt) == SQLITE_DONE) {
			db_success = true;
		}
		sqlite3_finalize(stmt);
	}
	sqlite3_close(db);

	// remove from list if removed from db
	if (db_success) {
		for (auto it = cat_profiles.begin(); it != cat_profiles.end(); ++it) {
			if (it->id == id) {
				cat_profiles.erase(it);
				if (DEBUG_MODE) Serial.printf("Cat ID %u removed.\n", id);
				return true;
			}
		}	
	}
	return false;
}

uint32_t map_rfid_to_cat(uint32_t rfid) {
	for (const auto& cat : cat_profiles) {
		if (cat.rfid == rfid) {
			return cat.id;
		}
	}
	return 0;
}

uint32_t map_weight_to_cat(float weight) {
	uint32_t best_id = 0;
	float smallest_diff = std::numeric_limits<float>::infinity();
	for (const auto& cat : cat_profiles) {
		float diff = abs(cat.weight - weight);
		if (diff < smallest_diff) {
			smallest_diff = diff;
			best_id = cat.id;
		}
	}
	return best_id;
}

// returns schedule id, or 0 if failed
int add_schedule(uint64_t base_time, float grams) {
	if (!connected) return 0;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return 0;
	}

	uint32_t new_id = 0;
	const char* sql = "INSERT INTO " SCHEDULE_TABLE " (base_time, grams) VALUES (?, ?);";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, base_time);
		sqlite3_bind_double(stmt, 2, grams);

		if (sqlite3_step(stmt) == SQLITE_DONE) {
			new_id = (uint32_t)sqlite3_last_insert_rowid(db);

			RepeatingSchedule s;
			s.id = new_id;
			s.base_time = base_time;
			s.grams = grams;

			// Use local time to determine if schedule fires today or tomorrow
			struct tm timeinfo;
			uint64_t now = get_time();
			uint64_t today = now / INTERVAL_24H;
			uint64_t local_seconds = 0;
			if (getLocalTime(&timeinfo)) {
				local_seconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
			}

			int insert_pos = 0;
			while (insert_pos < (int)schedules.size() && schedules[insert_pos].base_time < base_time) {
				insert_pos += 1;
			}
			schedules.insert(schedules.begin() + insert_pos, s);

			// If schedule hasn't passed yet today, allow it to fire today
			if (local_seconds < base_time) {
				schedule_idx = insert_pos;
				interval_num = today - 1; // allow firing today
			} else {
				// Already past today — fire tomorrow
				if (insert_pos <= schedule_idx) schedule_idx += 1;
			}
			if (DEBUG_MODE) Serial.printf("Schedule added. idx=%d interval_num=%llu\n", schedule_idx, interval_num);
		} else {
			if (DEBUG_MODE) Serial.println("Failed to add schedule.");
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return new_id;
}

bool remove_schedule(uint32_t id) {
	if (!connected) return false;
	sqlite3 *db;
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		sqlite3_close(db);
		connected = false;
		return false;
	}

	// remove from db
	sqlite3_stmt *stmt;
	const char* sql = "DELETE FROM " SCHEDULE_TABLE " WHERE id = ?;";
	bool db_success = false;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, id);
		if (sqlite3_step(stmt) == SQLITE_DONE) {
			db_success = true;
		}
		sqlite3_finalize(stmt);
	}
	sqlite3_close(db);

	// remove from list if removed from db
	if (db_success) {

		for (int i = 0; i < schedules.size(); ++i) {
			if (schedules[i].id == id) {

				if (i < schedule_idx) {
					schedule_idx -= 1;
				} else if (i == schedule_idx) {
					if (schedule_idx == schedules.size() - 1) {
						schedule_idx = 0;
						interval_num += 1;
					}
				}

				schedules.erase(schedules.begin() + i);
				if (DEBUG_MODE) Serial.printf("Schedule ID %u removed.\n", id);
				return true;
			}
		}
	}
	return false;
}

// emergency feeding when time is unknown
void backup_schedule() {
	static unsigned long em_last_fed = 0;
	constexpr unsigned long EM_DELAY = 5 * 60 * 1000; // 5 min delay after power on
	constexpr unsigned long EM_INTERVAL = 6 * 60 * 60 * 1000; // 6 hr intervals
	if (millis() > EM_DELAY) {
		if (em_last_fed == 0 || (millis() - em_last_fed > EM_INTERVAL)) {
			em_last_fed = millis();
			if (DEBUG_MODE) Serial.println("[EMERGENCY FEED] No time sync. Feeding on local schedule.");
			feed_grams(30.0); // feed 30g/6hrs = 120g/day
		}
	}
}

void handle_schedules() {
	if (!schedules_loaded) backup_schedule();
	if (schedules.empty()) return;

	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) return;
	uint64_t seconds_today = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
	uint64_t today = get_time() / INTERVAL_24H;

	if (schedule_idx >= (int)schedules.size()) schedule_idx = 0;
	auto& next = schedules[schedule_idx];

	if (seconds_today >= next.base_time && interval_num < today) {
		if (DEBUG_MODE) Serial.printf("[SCHED] Firing idx=%d base_time=%llu grams=%.1f\n",
			schedule_idx, next.base_time, next.grams);
		feed_grams(next.grams);

		schedule_idx += 1;
		if (schedule_idx >= (int)schedules.size()) {
			schedule_idx = 0;
			interval_num = today;
		}
		if ((int)schedules.size() == 1) interval_num = today;
	}
}