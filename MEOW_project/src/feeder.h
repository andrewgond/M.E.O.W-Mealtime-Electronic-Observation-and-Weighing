#pragma once
#include "config.h"
#include "network.h"
#include "scales.h"

#include <vector>
#include <stdint.h>

constexpr int   STEP_INTERVAL_US = 800;   // microseconds between steps
constexpr int   STEPS_PER_BURST  = 20;    // steps before checking scale
constexpr int   SETTLE_MS        = 200;   // ms to wait after burst — must be >100ms for HX711 at 10Hz
constexpr int   MAX_STEPS        = 10000; // hard safety limit (~50 full rotations)
constexpr float OVERSHOOT_MARGIN = 2.0f;  // stop early to account for inflight food

inline void _step_once() {
	digitalWrite(PIN_MOTOR_STEP, HIGH);
	delayMicroseconds(5);
	digitalWrite(PIN_MOTOR_STEP, LOW);
	delayMicroseconds(STEP_INTERVAL_US);
}

// ── Closed-loop dispense ──────────────────────────────────
// Spins motor in bursts, reads the bowl/hopper scale after each burst,
// stops when dispensed weight >= targetGrams.
// Returns actual grams dispensed.
//
// NOTE: The food scale is on the BOWL — weight goes UP as food is added.
// So dispensed = current - baseline (opposite of a hopper-mounted scale).
//
// TUNING:
//   Over target  → increase OVERSHOOT_MARGIN
//   Under target → decrease OVERSHOOT_MARGIN
//   Scale reads 0 during dispense → increase SETTLE_MS
float feed_grams(float targetGrams) {
	if (DEBUG_MODE) Serial.printf("[FEED] Dispensing %.1fg (closed-loop)\n", targetGrams);

	// Wait for scale to be ready and take baseline before dispensing
	unsigned long wait_start = millis();
	while (!scale_food.is_ready() && millis() - wait_start < 500);
	float baseline = scale_food.get_units(3);
	if (DEBUG_MODE) Serial.printf("[FEED] Baseline: %.2fg\n", baseline);

	float dispensed  = 0.0f;
	int   totalSteps = 0;
	float stopAt     = targetGrams - OVERSHOOT_MARGIN;

	digitalWrite(PIN_MOTOR_DIR, HIGH);

	while (totalSteps < MAX_STEPS) {
		// Run a burst of steps
		for (int i = 0; i < STEPS_PER_BURST; i++) _step_once();
		totalSteps += STEPS_PER_BURST;

		// Wait for food to settle and HX711 to complete its cycle
		// HX711 needs ~100ms per reading at 10Hz — SETTLE_MS must be > 100
		delay(SETTLE_MS);

		// Read how much has been added to the bowl
		// Weight goes UP so dispensed = current - baseline
		if (scale_food.is_ready()) {
			float current = scale_food.get_units(3);
			dispensed = current - baseline;  // bowl weight increased = food dispensed
			if (DEBUG_MODE) Serial.printf("[FEED] %.2fg / %.1fg\n", dispensed, targetGrams);
			if (dispensed >= stopAt) break;
		}
	}

	// Final accurate read after food fully settles
	delay(300);
	if (scale_food.is_ready()) {
		dispensed = scale_food.get_units(3) - baseline;
	}

	if (DEBUG_MODE) {
		if (totalSteps >= MAX_STEPS) Serial.println("[FEED] Safety limit reached");
		Serial.printf("[FEED] Done — %.2fg dispensed in %d steps\n", dispensed, totalSteps);
	}

	return dispensed;
}
