#include "one_wire_request.h"
#include "maxim_crc.h"
#include <util/delay.h>

#define NUM_RETRIES 5
// both in milliseconds
#define RETRY_INITIAL_DELAY 0
#define RETRY_DELAY_BACKOFF 2

inline uint8_t dallas_request_base(DALLAS_IDENTIFIER_t * id, uint16_t flags, uint8_t len, uint8_t * request, uint8_t response_len, uint8_t * response_buf) {
	uint8_t cur_byte;
	uint8_t bit_ctr;

	// Send the ROM command
	if (id) {
		dallas_match_rom(id);
	} else {
		dallas_skip_rom();
	}
	if (dallas_bus_error) return dallas_bus_error;

	// Write the request
	if (flags & DALLAS_REQ_LEN_BITS) {
		dallas_write_buffer(request, len >> 3);
		if (dallas_bus_error) return dallas_bus_error;
		if (len & 0x07) {
			cur_byte = request[len >> 3];
			for (bit_ctr = len & 0x07; bit_ctr; bit_ctr--) {
				dallas_write(cur_byte & 0x01);
				if (dallas_bus_error) return dallas_bus_error;
				cur_byte >>= 1;
			}
		}
	} else {
		dallas_write_buffer(request, len);
		if (dallas_bus_error) return dallas_bus_error;
	}

	// Read the response
	dallas_read_buffer(response_buf, response_len);

	// Read until 1 if flag is set
	if (flags & DALLAS_REQ_READ_UNTIL_1) {
		dallas_read_until_1();
		if (dallas_bus_error) return dallas_bus_error;
	}

	// Validate not all bytes are 0xff
	if (response_len && (flags & DALLAS_REQ_FAIL_ALL_ONES)) {
		for (cur_byte = 0; cur_byte < response_len; cur_byte++) {
			if (response_buf[cur_byte] != 0xff) break;
		}
		if (cur_byte == response_len) {
			return 0xC0;
		}
	}

	// Validate checksums
	if (flags & DALLAS_REQ_EXPECT_CKSUM8) {
		if (flags & DALLAS_REQ_CKSUM_INVERTED) {
			response_buf[response_len - 1] = ~(response_buf[response_len - 1]);
		}
		if (mcrc8_push_buf(0, response_buf, response_len)) {
			return 0xC1;
		}
	}
	if (flags & DALLAS_REQ_EXPECT_CKSUM16) {
		if (flags & DALLAS_REQ_CKSUM_INVERTED) {
			response_buf[response_len - 1] = ~(response_buf[response_len - 1]);
			response_buf[response_len - 2] = ~(response_buf[response_len - 2]);
		}
		if (mcrc16_push_buf(0, response_buf, response_len)) {
			return 0xC1;
		}
	}

	// Return success
	return 0;
}

uint8_t dallas_request(DALLAS_IDENTIFIER_t * id, uint16_t flags, uint8_t len, uint8_t * request, uint8_t response_len, uint8_t * response_buf) {
	uint8_t retry_ctr = 0;
	uint8_t cur_res;
	uint8_t i;
	for (;;) {
		cur_res = 0;
		if (!dallas_bus_error) {
			cur_res = dallas_request_base(id, flags, len, request, response_len, response_buf);
		}
		if (cur_res || dallas_bus_error) {
			retry_ctr++;
			if (retry_ctr > NUM_RETRIES || !(flags & DALLAS_REQ_RETRY)) {
				if (dallas_bus_error) {
					return dallas_bus_error;
				} else {
					return cur_res;
				}
			}
			if (flags & DALLAS_REQ_TXN) {
				dallas_end_txn();
			}
			_delay_ms(RETRY_INITIAL_DELAY);
			for (i = 0; i < retry_ctr; ++i) {
				_delay_ms(RETRY_DELAY_BACKOFF);
			}
			retry_ctr++;
			if (flags & DALLAS_REQ_TXN) {
				dallas_begin_txn();
			}
		} else {
			if (flags & DALLAS_REQ_TXN) {
				dallas_hold_txn();
			}
			return 0;
		}
	}
}

uint8_t dallas_request_txn(DALLAS_IDENTIFIER_t * id, uint16_t flags, uint8_t len, uint8_t * request, uint8_t response_len, uint8_t * response_buf) {
	dallas_begin_txn();
	if (dallas_bus_error) {
		dallas_end_txn();
		return dallas_bus_error;
	}
	uint8_t res = dallas_request(id, flags | DALLAS_REQ_TXN, len, request, response_len, response_buf);
	dallas_end_txn();
	return res;
}
