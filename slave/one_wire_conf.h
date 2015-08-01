#ifndef ONE_WIRE_CONF_H
#define ONE_WIRE_CONF_H

//#define F_CPU 8000000UL

// IO port for bus
#define DALLAS_PORT PORTA

// Input register for DALLAS_PORT
#define DALLAS_PORT_IN PINA

// Data direction register for DALLAS_PORT
#define DALLAS_DDR DDRA

// Pin number on the port
#define DALLAS_PIN 3

// Interrupt vector for the pin change interrupt for the pin
// Can comment this out to disable the interrupt
#define DALLAS_PCINT_VECT PCINT0_vect

// Pin Change Interrupt Mask Register
#define DALLAS_PCINT_MASK PCMSK0

// Which bit (LSB=0) in DALLAS_PCINT_MASK corresponds to this pin
#define DALLAS_PCINT_BIT 3

// Which bit in GIMSK corresponds to DALLAS_PCINT_VECT
#define DALLAS_GIMSK_BIT 4

// Which bit in GIFT corresponds to DALLAS_PCINT_VECT
#define DALLAS_GIFR_BIT 4

// Which timer mode to use
#define DALLAS_TIMER DALLAS_TIMER_0_8BIT
#define DALLAS_TIMER_VECT TIM0_COMPA_vect
//#define DALLAS_TIMER DALLAS_TIMER_1_16BIT
//#define DALLAS_TIMER_VECT TIM1_COMPA_vect

// Our own ID
// Define one of these two
#define OWS_ID { 0x88, 0x22, 0x44, 0xaa, 0xbb, 0x00, 0xff, 0x77 };
//#define OWS_ID_EEPROM_ADDR (const uint8_t *)0

// Debug LED
#define OWS_DEBUG_LED_DDR DDRA
#define OWS_DEBUG_LED_PORT PORTA
#define OWS_DEBUG_LED_PIN 7


#endif
