/*
 * An AVR library for communication on a Dallas 1-Wire bus.
 *
 * Features (?)
 * ------------
 *
 * * It uses a single GPIO pin (no UARTs).
 * * It does not use dynamic memory allocation. The only drawback is that you
 *   have to know the number of devices on the bus in advance.
 * * It is polled, not interrupt-driven. There are several sections of code
 *   that must run for a specific amount of time and have to disable
 *   interrupts globally.
 * * Only the MATCH_ROM, SEARCH_ROM, and SKIP_ROM commands have been
 *   implemented. At this point other commands would be trivial to add.
 *
 * Directions
 * ----------
 *
 * 1. Modify F_CPU, DALLAS_PORT, DALLAS_DDR, DALLAS_PORT_IN, DALLAS_PIN, and
 *    DALLAS_NUM_DEVICES to match your application.
 * 2. In your code, first run dallas_search_identifiers() to populate the
 *    the list of identifiers with the devices on your bus.
 * 3. ???
 * 4. Profit!
 *
 * Cautions/Caveats
 * ----------------
 *
 * The 1-Wire bus is *very* timing-dependent. If you are having issues and it's
 * not an electrical/connectivity one it is most likely a timing issue. The
 * worst function in this regard is dallas_read(). The delays chosen there are a
 * compromise between having to wait for the bus to return to 5V after being
 * pulled to ground (determined by the RC time constant) while also needing to
 * read the bus before the 15 usec time slot expires.
 *
 * Verify the timing with a logic analyzer or oscilloscope. Also check the
 * datasheet for your specific device to make sure that it is ok with the timing
 * values chosen in the code and modify, if necessary.
 *
 * I am not a 1-Wire expert by any means so this code is provided as-is.
 *
 * Enjoy!
 * ------
 *
 * April 30, 2010
 */

/******************************************************************************
 * Copyright © 2010, Mike Roddewig (mike@dietfig.org).
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <avr/io.h>

#include <stdint.h>

#include "dallas_one_wire.h"
#include "delay_helpers.h"

#include <util/atomic.h>
#include <util/delay.h>

//////////////////////
// Global variables //
//////////////////////

DALLAS_IDENTIFIER_LIST_t identifier_list;

/////////////////////////////////////
// Identifier routine return codes //
/////////////////////////////////////

#define DALLAS_IDENTIFIER_NO_ERROR 0x00
#define DALLAS_IDENTIFIER_DONE 0x01
#define DALLAS_IDENTIFIER_SEARCH_ERROR 0x02

#define pin_is_low() (!(DALLAS_PORT_IN & _BV(DALLAS_PIN)))
#define pin_is_high() (DALLAS_PORT_IN & _BV(DALLAS_PIN))

inline void set_bus_high() {
	// Set pin as input
	DALLAS_DDR &= ~_BV(DALLAS_PIN);
	// Make sure internal pullup is disabled
	DALLAS_PORT &= ~_BV(DALLAS_PIN);
}

inline void set_bus_low() {
	// Configure pin as output (should already be low if initialized)
	DALLAS_DDR |= _BV(DALLAS_PIN);
	DALLAS_PORT &= ~_BV(DALLAS_PIN);
}

// This flag is set to 1 when a bus error occurs
uint8_t dallas_bus_error = 0;

///////////////
// Functions //
///////////////

// Makes sure the bus is high for the duration of the given microseconds
// Returns 0 if the bus is high for the whole time, and 1 if the bus went low
inline uint8_t ensure_bus_high(uint8_t max_us) {
	// This loop takes 5 cycles without NOPs (on my compiler)
	max_us = DELAY_ROUND_UP(max_us, 5);
	for(;;) {
		if (pin_is_low()) return 1;
		//_NOP();
		//_NOP();
		//_NOP();
		//--max_us;
		DELAY_EXTRA_NOPS(5);
		max_us -= DELAY_DECR_USECS(5);
		if (!max_us) return 0;
	}
}

// Ensures that the bus is already high, or transitions to the high state at most
// once, for the duration of max_us.  Returns 0 if the bus transitions high at most
// once.  Returns 1 if the bus transitions back to low, or the bus never transitions
// high.
inline uint8_t ensure_bus_transition_high(uint8_t max_us) {
	max_us = DELAY_ROUND_UP(max_us, 5);
	// Wait until bus is high
	// Loop is 5 cycles without padding
	for(;;) {
		if (pin_is_high()) break;
		DELAY_EXTRA_NOPS(5);
		max_us -= DELAY_DECR_USECS(5);
		if (!max_us) return 1;
	}
	// Ensure the bus does not return low
	// Loop is 5 cycles without padding
	for(;;) {
		if (pin_is_low()) return 1;
		DELAY_EXTRA_NOPS(5);
		max_us -= DELAY_DECR_USECS(5);
		if (!max_us) return 0;
	}
}

// Can set dallas_bus_error flag
void dallas_write(uint8_t bit) {
	if (bit == 0x00) {
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			// Make sure the bus is high
			set_bus_high();
			if (pin_is_low()) { dallas_bus_error = 1; return; }

			set_bus_low();

			// Wait the required time.
			_delay_us(60);

			// Release the bus.
			set_bus_high();

			// Let the rest of the time slot expire.
			if (ensure_bus_transition_high(30)) { dallas_bus_error = 1; return; }
		}
	}
	else {
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			// Make sure the bus is high
			set_bus_high();
			if (pin_is_low()) { dallas_bus_error = 1; return; }

			set_bus_low();

			// Wait the required time.
			_delay_us(10);

			// Release the bus.
			set_bus_high();

			// Let the rest of the time slot expire.
			if (ensure_bus_transition_high(50)) { dallas_bus_error = 1; return; }
		}
	}
	return 0;
}

// Returns 0 or 1 on success
// Sets dallas_bus_error flag
uint8_t dallas_read(void) {
	uint8_t reply;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Make sure the bus is high
		DALLAS_DDR &= ~_BV(DALLAS_PIN);
		if (pin_is_low()) { dallas_bus_error = 1; return 0; }

		set_bus_low();

		// Wait the required time.
		_delay_us(2);

		set_bus_high();

		// Wait for a bit.
		_delay_us(13);

		if ((DALLAS_PORT_IN & _BV(DALLAS_PIN)) == 0x00) {
			reply = 0x00;
		}
		else {
			reply = 0x01;
		}

		// Let the rest of the time slot expire.
		if(ensure_bus_transition_high(45)) { dallas_bus_error = 1; return 0; }
	}

	return reply;
}

void dallas_setup() {
	DALLAS_PORT &= ~_BV(DALLAS_PIN);
}

// Resets the bus and returns 0x01 if a slave indicates present, 0x00 otherwise.
// Can set dallas_bus_error flag
uint8_t dallas_reset(void) {
	uint8_t reply;
	// Unset bus error
	dallas_bus_error = 0;

	// Ensure internal pullup is disabled
	DALLAS_PORT &= ~_BV(DALLAS_PIN);

	// Reset the slave_reply variable.
	reply = 0x00;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

		// Pull the bus low.
		set_bus_low();

		// Wait the required time.
		_delay_us(500); // 500 uS

		// Switch to an input and wait.
		set_bus_high();

		if (ensure_bus_transition_high(7)) { dallas_bus_error = 1; return 0; }

		if ((DALLAS_PORT_IN & _BV(DALLAS_PIN)) == 0x00) {
			reply = 0x02;
		} else {

			_delay_us(63);

			if ((DALLAS_PORT_IN & _BV(DALLAS_PIN)) == 0x00) {
				reply = 0x01;
			}

			if (ensure_bus_transition_high(420)) { dallas_bus_error = 1; return 0; }
		}
	}

	return reply;
}

void dallas_write_byte(uint8_t byte) {
	uint8_t position;

	for (position = 0x00; position < 0x08; position++) {
		dallas_write(byte & 0x01);
		if (dallas_bus_error) return;

		byte = (byte >> 1);
	}
}

uint8_t dallas_read_byte(void) {
	uint8_t byte;
	uint8_t position;

	byte = 0x00;

	for (position = 0x00; position < 0x08; position++) {
		byte += (dallas_read() << position);
		if (dallas_bus_error) return 0;
	}

	return byte;
}

// Uses the uC to power the bus.
void dallas_drive_bus(void) {
	// Configure the pin as an output.
	DALLAS_DDR |= _BV(DALLAS_PIN);

	// Set the bus high.
	DALLAS_PORT |= _BV(DALLAS_PIN);
}

void dallas_match_rom(DALLAS_IDENTIFIER_t * identifier) {
	uint8_t identifier_bit;
	uint8_t current_byte;
	uint8_t current_bit;

	dallas_reset();
	if (dallas_bus_error) return;
	dallas_write_byte(MATCH_ROM_COMMAND);
	if (dallas_bus_error) return;

	for (identifier_bit = 0x00; identifier_bit < DALLAS_NUM_IDENTIFIER_BITS; identifier_bit++) {
		current_byte = identifier_bit / 8;
		current_bit = identifier_bit - (current_byte * 8);

		dallas_write(identifier->identifier[current_byte] & _BV(current_bit));
		if (dallas_bus_error) return;
	}
}

void dallas_skip_rom(void) {
	dallas_reset();
	if (dallas_bus_error) return;
	dallas_write_byte(SKIP_ROM_COMMAND);
}

#define BYTE_OF_BIT(bit) ((uint8_t)bit >> 3)
#define BIT_OF_BIT(bit) ((uint8_t)bit & 0x07)

#define GET_IDENT_BIT(ident, bit) ( ident.identifier[BYTE_OF_BIT(bit)] & _BV(BIT_OF_BIT(bit)) )
#define SET_IDENT_BIT(ident, bit) ident.identifier[BYTE_OF_BIT(bit)] |= _BV(BIT_OF_BIT(bit))
#define CLR_IDENT_BIT(ident, bit) ident.identifier[BYTE_OF_BIT(bit)] &= ~_BV(BIT_OF_BIT(bit))
#define SYNC_IDENT_BIT(ident, bit, val) if(val) { SET_IDENT_BIT(ident, bit); } else { CLR_IDENT_BIT(ident, bit); }


//// Keep bitmask of bits where there was divergence.  After finishing a pass, traverse this in reverse and complete the pass at the last divergence.

uint8_t dallas_search_identifiers(void) {
	// Current device being filled in
	uint8_t current_device = 0;
	// Current bit of current_device being filled in
	// This corresponds to: identifier_list.identifiers[current_device].identifier[current_bit >> 3] |= _BV(7 - (current_bit % 8))
	int8_t current_bit;
	// Current set of 2 bits received
	uint8_t received_two_bits;
	// Flags of bits that have diverged in the current branch
	DALLAS_IDENTIFIER_t branch_diverge_flags;
	// Current bit that last diverged
	int8_t current_diverge_bit = -1;
	uint8_t current_bit_value;
	uint8_t current_byte;

	// Clear device list
	identifier_list.num_devices = 0;

	// Zero out branch_diverge_flags
	for (current_byte = 0; current_byte < DALLAS_NUM_IDENTIFIER_BITS / 8; current_byte++) {
		branch_diverge_flags.identifier[current_byte] = 0;
	}

	// Iterate multiple passes, discover one device per pass
	while(1) {
		current_bit = 0;
		dallas_reset();
		if (dallas_bus_error) return 3;
		dallas_write_byte(SEARCH_ROM_COMMAND);
		if (dallas_bus_error) return 3;
		// Iterate through all bits
		while(current_bit < DALLAS_NUM_IDENTIFIER_BITS) {
			received_two_bits = (dallas_read() << 1);
			if (dallas_bus_error) return 3;
			received_two_bits += dallas_read();
			if (dallas_bus_error) return 3;
			if (current_bit < current_diverge_bit) {
				// Follow the same path.  Ignore the received bits and go in the same previous direction.
				current_bit_value = GET_IDENT_BIT(identifier_list.identifiers[current_device - 1], current_bit);
				SYNC_IDENT_BIT(identifier_list.identifiers[current_device], current_bit, current_bit_value);
				dallas_write(current_bit_value);
			} else if (current_bit == current_diverge_bit) {
				// This was the bit we diverged on last time.  Last time, we took a 0 path.  Now we need to take a 1 path.
				SET_IDENT_BIT(identifier_list.identifiers[current_device], current_bit);
				dallas_write(0x01);
				// Clear the divergeance bit in the list (won't be relevant next path)
				CLR_IDENT_BIT(branch_diverge_flags, current_bit);
			} else if (received_two_bits == 0x01) {
				// All devices have 0 bits
				CLR_IDENT_BIT(identifier_list.identifiers[current_device], current_bit);
				dallas_write(0x00);
			} else if (received_two_bits == 0x02) {
				// All devices have 1 bits
				SET_IDENT_BIT(identifier_list.identifiers[current_device], current_bit);
				dallas_write(0x01);
			} else if (received_two_bits == 0x00) {
				// Some devices have 0's some have 1's
				// Set divergence flag and follow the 0 path
				SET_IDENT_BIT(branch_diverge_flags, current_bit);
				CLR_IDENT_BIT(identifier_list.identifiers[current_device], current_bit);
				dallas_write(0x00);
			} else {
				// No devices on this branch match?
				break;
			}
			if (dallas_bus_error) return 3;
			current_bit++;
		}
		// If we didn't complete a path, it was an error (or no device found)
		if (current_bit != DALLAS_NUM_IDENTIFIER_BITS) {
			return 1;
		}
		// Increment number of devices
		current_device++;
		if (current_device >= DALLAS_NUM_DEVICES) {
			return 2;
		}
		// Traverse branch_diverge_flags backwards until the diverge bit was found
		for (current_diverge_bit = DALLAS_NUM_IDENTIFIER_BITS - 1; current_diverge_bit >= 0; current_diverge_bit--) {
			if (GET_IDENT_BIT(branch_diverge_flags, current_diverge_bit)) {
				break;
			}
		}
		if (current_diverge_bit < 0) {
			// No divergence.  All done.
			identifier_list.num_devices = current_device;
			return 0;
		}
	}
}


DALLAS_IDENTIFIER_LIST_t * get_identifier_list(void) {
	return &identifier_list;
}


void dallas_write_buffer(uint8_t * buffer, uint8_t buffer_length) {
	uint8_t i;

	for (i = 0x00; i < buffer_length; i++) {
		dallas_write_byte(buffer[i]);
		if (dallas_bus_error) return;
	}
}

void dallas_read_buffer(uint8_t * buffer, uint8_t buffer_length) {
	uint8_t i;

	for (i = 0x00; i < buffer_length; i++) {
		buffer[i] = dallas_read_byte();
		if (dallas_bus_error) return;
	}
}

void dallas_hold_txn() {
	set_bus_low();
}

void dallas_begin_txn() {
	uint8_t ret;
	for (;;) {
		if (!ensure_bus_high(500)) break;
		_delay_us(123);
	}
	dallas_hold_txn();
}

void dallas_end_txn() {
	set_bus_high();
}
