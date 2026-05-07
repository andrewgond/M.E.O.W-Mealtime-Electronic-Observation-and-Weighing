#pragma once
#include <Arduino.h>

uint32_t get_rfid(HardwareSerial &serial) {
	uint32_t last_tag = 0;
	static byte buf[14];
	static int idx = 0;

	auto to_hex = [](byte a, byte b) {
		auto nibble = [](byte c) { return (c >= '0' && c <= '9') ? (c - '0') : (c - 'A' + 10); };
		return (nibble(a) << 4) | nibble(b);
	};

	while (serial.available() > 0) {
		byte incoming = serial.read();
		if (incoming == 0x02) idx = 0; // start byte
		if (idx < 14) buf[idx++] = incoming; // save to buffer

		// wait until buffer holds 14 items
		if (idx != 14) continue;

		// reset idx and operate on buffer
		idx = 0;
		if (buf[13] != 0x03) continue; // invalid end byte

		// checksum for valid tag
		byte checksum = 0;
		for (int i = 1; i < 11; i += 2) checksum ^= to_hex(buf[i], buf[i + 1]);
		if (checksum != to_hex(buf[11], buf[12])) continue; // invalid checksum
		
		// write from the buffer to the last tag recorded
		char id_str[9] = {0};
		memcpy(id_str, &buf[3], 8);
		last_tag = strtoul(id_str, NULL, 16);
	}
	return last_tag;
}
