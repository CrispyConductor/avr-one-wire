/** DOES NOT YET WORK **/
#ifndef DELAY_HELPERS_H
#define DELAY_HELPERS_H

#include <avr/cpufunc.h>
#include <util/delay_basic.h>

/*
These are helper macros for functions that need precise microsecond-level timing that
iterate and perform a synchronous operation each cycle.  These loops are typically
in the form:

void doStuffForMicroseconds(uint8_t microseconds) {
	microseconds = DELAY_ROUND_DOWN(microseconds, 6);
	for (; microseconds; microseconds -= DELAY_DECR_USECS(6)) {
		DELAY_EXTRA_NOPS(6)
		// Do operation
	}
}

The '6' in this example is the number of cycles consumed by the loop without any
extra inserted instructions.  This number of cycles must include any compares,
branches, and decrements, as well as the internal operation.

The DELAY_EXTRA_NOPS macro inserts extra NOPs to make the number of cycles per iteration
an even multiple of the frequency.

The DELAY_DECR_USECS macro returns the amount to decrement microseconds by each loop
iteration.

The DELAY_ROUND_DOWN or DELAY_ROUND_UP macros round microseconds to an even
multiple of DELAY_DECR_USECS so decrementing by DELAY_DECR_USECS won't make
the counter go below 0.

The F_CPU macro must be defined, and must be a multiple of 1,000,000 up to 16,000,000.

*/

// Divides numer by denom, rounding up (such that constant values will be optimized away)
// Result should be cast to the desired type
#define INT_DIV_ROUND_UP(numer, denom) (numer / denom + ((numer % denom) ? 1 : 0))

// Macro to round up to next highest power of 2 (for 8 bits)
// Note: There are more efficient ways to do this without preoptimized macros
#define NEXT_HIGHEST_POWER_OF_2(v) (((v-1)|((v-1)>>1)|((v-1)>>2)|((v-1)>>3)|((v-1)>>4)|((v-1)>>5)|((v-1)>>6)|((v-1)>>7))+1)

// Given a power of 2 (ie, a single bit set), returns a value with that bit, and all lower bits set
// Works for 8-bit numbers
#define POWER_OF_2_MASK(v) (v|(v>>1)|(v>>2)|(v>>3)|(v>>4)|(v>>5)|(v>>6)|(v>>7))

#define DELAY_CYCLES_EXACT_HELPER(n) if (n == 1) { _NOP(); } else if (n == 2) { _NOP(); _NOP(); }
#define DELAY_LOOP_1_ITERATIONS(n) ((uint8_t)(n / 3))
#define DELAY_CYCLES_EXACT(n) if (n > 2) { _delay_loop_1(DELAY_LOOP_1_ITERATIONS(n)); } if (n % 3) { DELAY_CYCLES_EXACT_HELPER(n % 3); }

// Number of cycles in each microsecond
#define CYCLES_PER_USEC (F_CPU / 1000000UL)

#define DELAY_DECR_USECS(loop_cycles) ((uint8_t)INT_DIV_ROUND_UP(loop_cycles, CYCLES_PER_USEC))
#define DELAY_NUM_EXTRA_CYCLES(loop_cycles) (DELAY_DECR_USECS(loop_cycles) * CYCLES_PER_USEC - loop_cycles)
#define DELAY_EXTRA_NOPS(loop_cycles) DELAY_CYCLES_EXACT(DELAY_NUM_EXTRA_CYCLES(loop_cycles))
#define DELAY_ROUND_DOWN(usecs, loop_cycles) (usecs & ~(POWER_OF_2_MASK(NEXT_HIGHEST_POWER_OF_2(DELAY_DECR_USECS(loop_cycles)))>>1))
#define DELAY_ROUND_UP(usecs, loop_cycles) (DELAY_ROUND_DOWN(usecs, loop_cycles) == usecs) ? usecs : ((usecs | (POWER_OF_2_MASK(NEXT_HIGHEST_POWER_OF_2(DELAY_DECR_USECS(loop_cycles)))>>1))+1)


#endif
