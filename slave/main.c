//#define F_CPU 8000000UL
#include <avr/io.h>
#include "./one_wire_slave.h"

void handle_ows_command(uint8_t command) {
	if (command == 0x11) {
		ows_write_buf("\xdd\xfe\xaa", 3);
	}
}

int main(void) {
	DDRA = 0xff;
	PORTA = 0xff;
	ows_setup();
	while(1) {
		//ows_check();
	}
}