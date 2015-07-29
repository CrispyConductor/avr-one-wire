#include "./one_wire_slave.h"
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <stdint.h>





#ifdef OWS_DEBUG_LED_PORT
#define led_on() OWS_DEBUG_LED_PORT &= ~_BV(OWS_DEBUG_LED_PIN);
#define led_off() OWS_DEBUG_LED_PORT |= _BV(OWS_DEBUG_LED_PIN);
inline void led_blink(uint8_t n) {
	uint8_t i;
	for (i = 0; i < n; i++) {
		led_on();
		_delay_ms(300);
		led_off();
		_delay_ms(300);
	}
}

inline void led_blink_bit(uint8_t b) {
	if (b) {
		led_on();
		_delay_ms(800);
		led_off();
		_delay_ms(200);
	} else {
		led_on();
		_delay_ms(200);
		led_off();
		_delay_ms(800);
	}
}

inline void led_blink_byte(uint8_t b) {
	uint8_t p;
	for(p = 0x80; p; p >>= 1) {
		led_blink_bit(b & p);
	}
}
#else
#define led_on()
#define led_off()
#define led_blink(n)
#define led_blink_byte(b)
#endif




// Our device identifier
OWS_IDENTIFIER_t ows_id;
// Flag is set during processing a transaction if an error occurs
// 1 = reset, 2 = timeout
#define OWS_ERROR_RESET 1
#define OWS_ERROR_TIMEOUT 2
uint8_t ows_error_flag = 0;

#define pin_is_low() (!(DALLAS_PORT_IN & _BV(DALLAS_PIN)))
#define pin_is_high() (DALLAS_PORT_IN & _BV(DALLAS_PIN))

inline void clear_pin_isr() {
	GIFR = 0;
}

inline void ows_bus_high() {
	// Set pin as input
	DALLAS_DDR &= ~_BV(DALLAS_PIN);
}

inline void ows_bus_low() {
	// Configure pin as output (should already be low if initialized)
	DALLAS_DDR |= _BV(DALLAS_PIN);
}


inline void wait_until_low() {
	for(;;) {
		if (pin_is_low()) return;
	}
}

inline void wait_until_high() {
	for(;;) {
		if (pin_is_high()) return;
	}
}

// Waits for up to max_us .  Returns 0 if pin is low.  Returns 1 if max is hit.
inline uint8_t wait_until_low_timed(uint8_t max_us) {
	// This loop takes 5 cycles without NOPs (on my compiler)
	for(;;) {
		if (pin_is_low()) return 0;
		_NOP();
		_NOP();
		_NOP();
		--max_us;
		if (!max_us) return 1;
	}
}

// Waits for up to max_us.  Returns 0 if pin is high.  Returns 1 if max is hit.
inline uint8_t wait_until_high_timed(uint8_t max_us) {
	// This loop takes 5 cycles without NOPs (on my compiler)
	for(;;) {
		if (pin_is_high()) return 0;
		_NOP();
		_NOP();
		_NOP();
		--max_us;
		if (!max_us) return 1;
	}
}

// Waits in units of 100 microseconds
uint8_t wait_until_low_timed_ex(uint8_t timeval) {
	for(;;) {
		if (!wait_until_low_timed(100)) return 0;
		timeval--;
		if (!timeval) return 1;
	}
}

inline void time_bus_low(uint8_t us) {
	ows_bus_low();
	//delay_unless_reset(us);
	_delay_us(us);
	ows_bus_high();
}

// Read 1 bit from the master (a WRITE 0 or WRITE 1)
// Returns 0 if 0 bit, 1 if 1 bit
// Also sets ows_error_flag
inline uint8_t ows_read_bit_internal() {
	if (wait_until_low_timed_ex(100)) {
		ows_error_flag = OWS_ERROR_TIMEOUT;
		return 0;
	}
	_delay_us(20);
	if (pin_is_low()) {
		if (wait_until_high_timed(250)) {
			ows_error_flag = OWS_ERROR_RESET;
		}
		return 0;
	} else {
		return 1;
	}
}

uint8_t ows_read_bit() {
	return ows_read_bit_internal();
}

// Read 1 byte from the master
inline uint8_t ows_read_byte_internal() {
	uint8_t bit;
	uint8_t ret = 0;
	for (bit = 0x01; bit; bit <<= 1) {
		if (ows_error_flag) return 0;
		if (ows_read_bit_internal()) {
			ret |= bit;
		}
	}
	return ret;
}

uint8_t ows_read_byte() {
	return ows_read_byte_internal();
}

// Writes 1 bit back to the master
// This is the equivalent of a READ 0 or READ 1 command
// Also sets ows_error_flag
inline void ows_write_bit_internal(uint8_t bit) {
	if (wait_until_low_timed_ex(100)) {
		ows_error_flag = OWS_ERROR_TIMEOUT;
		return;
	}
	if (bit) {
		// Leave bus high for at least 20 us
		_delay_us(20);
	} else {
		// Pull bus low for at least 20 us
		time_bus_low(20);
	}
	// Allow bus to return high
	if (wait_until_high_timed(250)) {
		ows_error_flag = OWS_ERROR_RESET;
	}
}

void ows_write_bit(uint8_t bit) {
	ows_write_bit_internal(bit);
}

// Writes 1 byte to the master
inline void ows_write_byte_internal(uint8_t b) {
	uint8_t bit;
	for (bit = 0x01; bit; bit <<= 1) {
		if (ows_error_flag) return;
		ows_write_bit_internal(b & bit);
	}
}

void ows_write_byte(uint8_t b) {
	ows_write_byte_internal(b);
}

// Reads len bytes into buffer buf.
// Returns nonzero on error.
uint8_t ows_read_buf(uint8_t * buf, uint8_t len) {
	while (len) {
		*buf = ows_read_byte_internal();
		if (ows_error_flag) return 1;
		++buf;
		--len;
	}
	return 0;
}

// Writes bytes out.
// Returns nonzero on error.
uint8_t ows_write_buf(uint8_t * buf, uint8_t len) {
	while (len) {
		ows_write_byte_internal(*buf);
		if (ows_error_flag) return 1;
		++buf;
		--len;
	}
	return 0;
}

// Returns 1 if device is selected and should read a device command
// Returns 0 if device is not selected
// ows_error_flag is set if relevant
inline uint8_t ows_handle_rom_command(uint8_t command) {
	uint8_t i, bit, bitVal, direction, b;
	uint8_t *id;
	switch(command) {
		case OWS_READ_ROM_COMMAND:
			for (i = 0; i < 8; i++) {
				ows_write_byte(ows_id.identifier[i]);
				if (ows_error_flag) return 0;
			}
			return 0;
		case OWS_SKIP_ROM_COMMAND:
			return 1;
		case OWS_MATCH_ROM_COMMAND:
			for (i = 0; i < 8; i++) {
				b = ows_read_byte();
				if (ows_error_flag) return 0;
				if (b != ows_id.identifier[i]) return 0;
			}
			return 1;
		case OWS_SEARCH_ROM_COMMAND:
			id = ows_id.identifier;
			for (i = 0; i < 8; i++) {
				for (bit = 0x01; bit; bit <<= 1) {
					bitVal = id[i] & bit;
					ows_write_bit(bitVal);
					if (ows_error_flag) return 0;
					ows_write_bit(!bitVal);
					if (ows_error_flag) return 0;
					// Don't read direction on last bit
					if (i != 7 || bit != 0x80) {
						direction = ows_read_bit();
						if (ows_error_flag) return 0;
						if ((!direction) != !(bitVal)) {
							return 0;
						}
					}
				}
			}
			return 0;
	}
	return 0;
}

// Called immediately after a reset
inline void ows_handle_reset() {
	ows_error_flag = 0;
	// Wait until the pin returns to high
	wait_until_high();
	// Reset pulse finished.  Wait and emit the presence pulse.
	_delay_us(20);
	// Emit the presence pulse
	time_bus_low(120);
	// Wait for bus to recover
	if (wait_until_high_timed(250)) {
		ows_error_flag = OWS_ERROR_RESET;
		return;
	}
	// Read ROM command
	uint8_t rom_command = ows_read_byte();
	if (ows_error_flag) return;
	if (!ows_handle_rom_command(rom_command)) return;
	if (ows_error_flag) return;
	// Read the device command
	uint8_t dev_command = ows_read_byte();
	handle_ows_command(dev_command);
}

inline void ows_handle_reset_isr() {
	for(;;) {
		if (!pin_is_low()) return;
		ows_handle_reset();
		if (ows_error_flag != OWS_ERROR_RESET) return;
	}
}


// Top value of timer for reset
#define TIMER_OCR_VALUE_RESET 255 // 255us with clk/8

#if DALLAS_TIMER == DALLAS_TIMER_0_8BIT
// These bits are normally all 0; only the CS bits are set when the timer is running
#define TIMER_IS_RUNNING() TCCR0B
// Value of TCCR0B register to run the timer for Timer 0, 8mhz
#define TIMER_ON_REG 0b00000010	// clk/8
// Value of TCCR0B register when timer is stopped
#define TIMER_OFF_REG 0b00000000
#elif DALLAS_TIMER == DALLAS_TIMER_1_16BIT
#define TIMER_IS_RUNNING() (TCCR1B & 0x07)
#define TIMER_ON_REG 0b00001010	// clk/8
#define TIMER_OFF_REG 0b00001000
#endif

inline void start_timer() {
#if DALLAS_TIMER == DALLAS_TIMER_0_8BIT
	TCCR0B = TIMER_ON_REG;
#elif DALLAS_TIMER == DALLAS_TIMER_1_16BIT
	TCCR1B = TIMER_ON_REG;
#endif
}

inline void stop_timer() {
#if DALLAS_TIMER == DALLAS_TIMER_0_8BIT
	TCCR0B = TIMER_OFF_REG;
	TCNT0 = 0;
#elif DALLAS_TIMER == DALLAS_TIMER_1_16BIT
	TCCR1B = TIMER_OFF_REG;
	TCNT1 = 0;
#endif
}

void handle_pin_isr() {
	stop_timer();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		ows_handle_reset_isr();
	}
}

#ifdef DALLAS_TIMER_VECT
ISR(DALLAS_TIMER_VECT) {
	handle_pin_isr();
}
#endif

#ifdef DALLAS_PCINT_VECT
ISR(DALLAS_PCINT_VECT) {
	if (pin_is_high()) {
		// Stop timer (pin went high)
		stop_timer();
	} else {
		// Start timer (pin went low)
		// Multiple triggers won't reset the value
		start_timer();
	}
}
#endif

void ows_setup_timer() {
#if DALLAS_TIMER == DALLAS_TIMER_0_8BIT
	// Disable timer output pins.  Enable CTC mode.
	TCCR0A = 0b00000010;
	// No force output comare.  Also enable CTC mode.  Initially disable timer.
	TCCR0B = 0b00000000;
	// Reset timer and output compare values
	TCNT0 = 0;
	OCR0A = 0;
	OCR0B = 0;
	// Enable the timer interrupt (won't be active until timer is started)
	TIMSK0 = 0b00000010;
	// Initialize the output compare register
	OCR0A = TIMER_OCR_VALUE_RESET;
#elif DALLAS_TIMER == DALLAS_TIMER_1_16BIT
	TCCR1A = 0b00000000;
	TCCR1B = 0b00001000;
	TCCR1C = 0;
	TCNT1 = 0;
	OCR1A = 0;
	OCR1B = 0;
	TIMSK1 = 0b00000010;
	OCR1A = TIMER_OCR_VALUE_RESET;
#else
#error "Invalid value of DALLAS_TIMER"
#endif
}

void ows_setup() {
	// Initialize ows_id
	uint8_t i;
	uint8_t ows_id_src[] = OWS_ID;
	for (i = 0; i < 8; i++) {
		ows_id.identifier[i] = ows_id_src[i];
	}

	// Initialize the timer
	ows_setup_timer();

	// Initialize the debug LED
#ifdef OWS_DEBUG_LED_PORT
	OWS_DEBUG_LED_DDR |= _BV(OWS_DEBUG_LED_PIN);
	led_off();
	led_on();
	_delay_ms(1000);
	led_off();
	_delay_ms(100);
	led_on();
	_delay_ms(100);
	led_off();
#endif

	// Initialize the pin and interrupt
	// Configure pin as input (initially)
	DALLAS_DDR &= ~_BV(DALLAS_PIN);
	// Set the port value to 0.  This is ALWAYS 0.  When the bus should be high,
	// the pin is configured as input to tri-state it and use the external
	// pullup, so this value of 0 disabled the internal pullup.  When the bus
	// should be low, this value drives the bus low.
	DALLAS_PORT &= ~_BV(DALLAS_PIN);
	// Enable the pin interrupt
	DALLAS_PCINT_MASK |= _BV(DALLAS_PCINT_BIT);
	// Enable the pin change interrupt set
	GIMSK |= _BV(DALLAS_GIMSK_BIT);

	// Enable interrupts
	sei();
}
