#pragma once
#include "HX711.h"

HX711 scale_food;
HX711 scale_platform;

constexpr float ZERO_THRESHOLD_FOOD = 1.0; // TODO
constexpr float ZERO_THRESHOLD_PLATFORM = 1.0; // TODO

constexpr int SCALE_BUFFER_SIZE = 10; // must be greater than 0

std::array<float, SCALE_BUFFER_SIZE> food_buf = {0};
int food_idx = 0;
std::array<float, SCALE_BUFFER_SIZE> platform_buf = {0};
int platform_idx = 0;

void _tick_scale(HX711 &scale, std::array<float, SCALE_BUFFER_SIZE> &buf, int &idx) {
	if (!scale.is_ready()) return;
	buf[idx] = scale.get_units(); // TODO: zero point that shifts over time, not really a tare, but similar to get values close to 0
	idx = (idx + 1) % SCALE_BUFFER_SIZE;
}

void tick_scales() {
	_tick_scale(scale_food, food_buf, food_idx);
	_tick_scale(scale_platform, platform_buf, platform_idx);
}

float _get_weight(std::array<float, SCALE_BUFFER_SIZE> &buf, float zero_threshold) {
	float total = 0;
	for (int i = 0; i < SCALE_BUFFER_SIZE; ++i) total += buf[i];
	float weight = total / SCALE_BUFFER_SIZE;
	return weight > zero_threshold ? weight : 0;
}

float get_food_weight() {
	return _get_weight(food_buf, ZERO_THRESHOLD_FOOD);
}

float get_platform_weight() {
	return _get_weight(platform_buf, ZERO_THRESHOLD_PLATFORM);
}

void tare_scales() {
	scale_food.tare();
	scale_platform.tare();
}

// returns range of the buffer. If values are all zero, it will return an infinite range.
float _get_stability(std::array<float, SCALE_BUFFER_SIZE> &buf, float zero_threshold) {
	float min = buf[0];
	float max = buf[0];
	for (int i = 1; i < SCALE_BUFFER_SIZE; ++i) {
		if (buf[i] < min) min = buf[i];
		if (buf[i] > max) max = buf[i];
	}
	if (max < zero_threshold) return std::numeric_limits<float>::infinity();
	return max - min;
}

float get_food_stability() {
	return _get_stability(food_buf, ZERO_THRESHOLD_FOOD);
}

float get_platform_stability() {
	return _get_stability(platform_buf, ZERO_THRESHOLD_PLATFORM);
}
