#ifndef ONE_WIRE_SLAVE_H
#define ONE_WIRE_SLAVE_H

#include <stdint.h>

// Timer defines
#define DALLAS_TIMER_0_8BIT 1
#define DALLAS_TIMER_1_16BIT 2

// One Wire Commands

#define OWS_MATCH_ROM_COMMAND 0x55
#define OWS_SKIP_ROM_COMMAND 0xCC
#define OWS_SEARCH_ROM_COMMAND 0xF0
#define OWS_READ_ROM_COMMAND 0x33

typedef struct {
	uint8_t identifier[8];
} OWS_IDENTIFIER_t;

#include "./one_wire_conf.h"


// Defined functions
void ows_setup();
uint8_t ows_read_buf(uint8_t * buf, uint8_t len);
uint8_t ows_write_buf(uint8_t * buf, uint8_t len);
void ows_write_byte(uint8_t b);
uint8_t ows_read_byte();
void handle_pin_isr();

// Functions to implement
void handle_ows_command(uint8_t command);

extern uint8_t ows_error_flag;

#endif

