#ifndef MAXIM_CRC_H
#define MAXIM_CRC_H

// Routines for dealing with maxim crc codes

// Adds 1 bit to the crc, returns the new crc
// Bit must be 0 or 1 (not 2, or 30, or whatever)
inline uint8_t mcrc8_push_bit(uint8_t crc, uint8_t bit) {
	uint8_t cur_8th_stage = crc & 0x01;
	uint8_t input_bit = bit ^ cur_8th_stage;
	// Rotate the crc
	crc >>= 1;
	// If the bit is a 1, the middle bits needs to be flipped and a 1 shifted in
	if (input_bit) {
		crc ^= 0x8C;
	}
	return crc;
}

// Adds 1 byte to the crc (lsb first), returns the new crc
// To generate a crc, set the initial crc to 0
// To validate a crc, set the initial crc to the one to validate, and ensure the
// eventual result is 0x00 .
inline uint8_t mcrc8_push_byte(uint8_t crc, uint8_t byte) {
	uint8_t ctr;
	for (ctr = 8; ctr; --ctr) {
		crc = mcrc8_push_bit(crc, byte & 0x01);
		byte >>= 1;
	}
	return crc;
}

inline uint8_t mcrc8_push_buf(uint8_t crc, uint8_t * buf, uint8_t len) {
	while (len) {
		crc = mcrc8_push_byte(crc, *buf);
		++buf;
		--len;
	}
	return crc;
}

#endif
