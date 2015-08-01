//#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "./one_wire_slave.h"
#include "./dallas_one_wire.h"

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

DALLAS_IDENTIFIER_t id_buf;

void test_master() {
	uint8_t buf[2];
	dallas_begin_txn();
	memcpy(id_buf.identifier, "\x28\xb4\xc5\xb0\x06\x00\x00\x75", 8);
	uint8_t r = dallas_reset();
	dallas_match_rom(&id_buf);
	dallas_write_byte(0x44);
	dallas_hold_txn();
	_delay_us(100);
	dallas_reset();
	dallas_match_rom(&id_buf);
	dallas_write_byte(0xbe);
	dallas_read_buffer(buf, 2);
	dallas_hold_txn();
	led_blink_byte(buf[1]);
	led_blink_byte(buf[0]);
	dallas_end_txn();
}

void handle_ows_command(uint8_t command) {
	if (command == 0x11) {
		ows_write_buf("\xdd\xfe\xaa", 3);
	} else if(command == 0x22) {
		ows_write_buf("\x01", 1);
		_delay_ms(20);
		test_master();
	}
}

int main(void) {
	DDRA = 0xff;
	PORTA = 0xff;
	ows_setup();
	dallas_setup();
	while(1) {
		//ows_check();
	}
}