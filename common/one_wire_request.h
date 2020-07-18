#ifndef ONE_WIRE_REQUEST_H
#define ONE_WIRE_REQUEST_H

#include "dallas_one_wire.h"

/*** REQUEST FLAGS ***/
// The 'len' parameter (request length) is specified in bits
#define DALLAS_REQ_LEN_BITS 0x01
// Request is executed as part of a transaction
#define DALLAS_REQ_TXN 0x02
// After the request is complete, keep issuing read timeslots until a `1` is read
#define DALLAS_REQ_READ_UNTIL_1 0x04
// Expect the last byte of the response to be a Maxim checksum
#define DALLAS_REQ_EXPECT_CKSUM8 0x08
// Expect the last 2 bytes of the response to be a Maxim checksum
#define DALLAS_REQ_EXPECT_CKSUM16 0x10
// Retry the request on failure
#define DALLAS_REQ_RETRY 0x20
// Consider the request a failure if the response is all 1's
#define DALLAS_REQ_FAIL_ALL_ONES 0x40
// Invert the checksum before checking
#define DALLAS_REQ_CKSUM_INVERTED 0x80

// Sends a "request" to a slave device.  Returns zero on success.
// If id is null, does a skip rom
uint8_t dallas_request(DALLAS_IDENTIFIER_t * id, uint16_t flags, uint8_t len, uint8_t * request, uint8_t response_len, uint8_t * response_buf);

// Performs the request inside of a new transaction
uint8_t dallas_request_txn(DALLAS_IDENTIFIER_t * id, uint16_t flags, uint8_t len, uint8_t * request, uint8_t response_len, uint8_t * response_buf);

#endif
