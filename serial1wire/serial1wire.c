#include "./dallas_one_wire.h"
//#define F_CPU 16000000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>

void writeStr(char *str) {
	int i;
	for(i = 0; str[i]; i++) {
		while(!(UCSR0A & (1 << 5))) {}
		_delay_ms(5);
		UDR0 = str[i];
	}
}

void writeNL() {
	writeStr("\r\n");
}

void writeBin(uint8_t b) {
	int i;
	char str[9];
	for(i = 0; i < 8; i++) {
		str[i] = (b & (1 << (7 - i))) ? '1' : '0';
	}
	str[8] = 0;
	writeStr(str);
}

void writeHex(uint8_t b) {
	char str[3];
	unsigned char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
	str[0] = digits[b >> 4];
	str[1] = digits[b & 0x0F];
	str[2] = 0;
	writeStr(str);
}

#define LINEBUF_LEN 256
unsigned char lineBuf[LINEBUF_LEN + 1];
uint16_t lineBufPos = 0;

unsigned char *readLine() {
	uint8_t b;
	while(1) {
		while(!(UCSR0A & _BV(7)));
		b = UDR0;
		if(b == '\r' || b == '\n') {
			if(lineBufPos) {
				lineBuf[lineBufPos] = 0;
				lineBufPos = 0;
				return lineBuf;
			}
		} else if(lineBufPos >= LINEBUF_LEN) {
			lineBuf[lineBufPos] = 0;
			lineBufPos = 0;
			return lineBuf;
		} else {
			lineBuf[lineBufPos++] = b;
		}
	}
}

typedef struct {
	unsigned char cmd;
	uint8_t * data;
	uint16_t datalen;
} command_t;

uint8_t resultBuf[512];
uint16_t resultBufPos = 0;
DALLAS_IDENTIFIER_t currentIdentifier;

void runCmd(unsigned char cmd, uint8_t * data, uint16_t datalen) {
	uint16_t i, j;
	uint8_t curResult;
	uint8_t lenToRead;
	DALLAS_IDENTIFIER_LIST_t * ids;
	// K W 44
	// K W be R 09
	switch(cmd) {
		case 'P':
			writeStr("PONG\r\n");
			for (i = 0; i < datalen; i++) {
				writeHex(data[i]);
			}
			writeStr("\r\n");
			break;
		case 'E':
			resultBuf[resultBufPos++] = dallas_reset();
			break;
		case 'D':
			dallas_drive_bus();
			resultBuf[resultBufPos++] = 1;
			break;
		case 'S':
			curResult = dallas_search_identifiers();
			resultBuf[resultBufPos++] = curResult;
			ids = get_identifier_list();
			for (i = 0; i < ids->num_devices; i++) {
				for (j = 0; j < 8; j++) {
					resultBuf[resultBufPos++] = ids->identifiers[i].identifier[j];
				}
			}
			break;
		case 'N':
			resultBuf[resultBufPos++] = dallas_search_identifiers();
			resultBuf[resultBufPos++] = get_identifier_list()->num_devices;
		case 'K':
			dallas_skip_rom();
			break;
		case 'I':
			if (datalen != 8) {
				writeStr("I must have 8 byte id\r\n");
				break;
			}
			for (i = 0; i < 8; i++) {
				currentIdentifier.identifier[i] = data[i];
			}
			break;
		case 'O':
			for (i = 0; i < 8; i++) {
				resultBuf[resultBufPos++] = currentIdentifier.identifier[i];
			}
			break;
		case 'M':
			dallas_match_rom(&currentIdentifier);
			break;
		case 'W':
			for (i = 0; i < datalen; i++) {
				dallas_write_byte(data[i]);
			}
			break;
		case 'R':
			if (datalen != 1) {
				writeStr("R must have length to read\r\n");
				break;
			}
			lenToRead = data[0];
			for (i = 0; i < lenToRead; i++) {
				resultBuf[resultBufPos++] = dallas_read_byte();
			}
			break;
/*		case 'T':
			writeStr("Starting test\r\n");
			dallas_search_identifiers();
			currentIdentifier = get_identifier_list()->identifiers[0];
			dallas_reset();
			dallas_match_rom(&currentIdentifier);
			dallas_write_byte(0xbe);
			lenToRead = 9;
			for (i = 0; i < lenToRead; i++) {
				resultBuf[resultBufPos++] = dallas_read_byte();
			}
			writeStr("Test done\r\n");
			break;
		case 'Y':
			writeStr("Starting test\r\n");
			//dallas_search_identifiers();
			currentIdentifier = get_identifier_list()->identifiers[0];
			dallas_reset();
			dallas_match_rom(&currentIdentifier);
			dallas_write_byte(0xbe);
			lenToRead = 9;
			for (i = 0; i < lenToRead; i++) {
				resultBuf[resultBufPos++] = dallas_read_byte();
			}
			writeStr("Test done\r\n");
			break;*/
		default:
			writeStr("Unrecognized Command: ");
			writeHex(cmd);
			writeStr("\r\n");
			break;
	}
}

command_t commandBuf[25];
uint8_t numCommands;

void runCommands() {
	uint8_t i;
	command_t * curCmd = commandBuf;
	resultBufPos = 0;
	char printBuf[50];
	sprintf(printBuf, "Executing %d commands\r\n", numCommands);
	writeStr(printBuf);
	for (i = 0; i < numCommands; i++) {
		runCmd(curCmd->cmd, curCmd->data, curCmd->datalen);
		curCmd++;
	}
	writeStr("Done executing commands\r\n");
	if (resultBufPos) {
		writeStr("Results:\r\n");
		for (i = 0; i < resultBufPos; i++) {
			writeHex(resultBuf[i]);
			writeStr(" ");
		}
		writeStr("\r\n");
	}
}

// Commands are single-letters followed by a series of optionally space-separated hex digits.
// All commands are uppercase.  Hex digits are lowercase.
// There can be multiple commands per line.
// This transforms lines into commandBuf .  Data is transformed in-place, overwriting lineBuf.
void readCmdLine() {
	writeStr("Commands: ");
	unsigned char * buf = readLine();
	writeStr((char *)buf);
	writeStr("\r\n");
	uint16_t bufReadPos = 0;
	uint16_t bufWritePos = 0;
	numCommands = 0;
	command_t * curCommand = 0;
	// 0 = start
	// 1 = waiting for command or data
	// 2 = waiting for hex digit part 2
	uint8_t state = 0;
	char c;
	// 0 = unknown
	// 1 = command
	// 2 = lower hex
	// 3 = upper hex
	uint8_t charType;
	uint8_t nibble;
	while (1) {
		c = buf[bufReadPos++];
		if (!c) break;
		if (c >= 'A' && c <= 'Z') charType = 1;
		else if (c >= '0' && c <= '9') charType = 2;
		else if (c >= 'a' && c <= 'f') charType = 3;
		else charType = 0;
		if (state == 0) { // START
			if (charType == 1) { // command
				curCommand = commandBuf + (numCommands++);
				curCommand->cmd = c;
				curCommand->data = (uint8_t *)buf + bufWritePos;
				curCommand->datalen = 0;
				state = 1;
			}
		} else if (state == 1 || state == 2) { // WAITING_COMMAND_OR_DATA or WAITING_FOR_NIBBLE_2
			if (charType == 1) { // command
				curCommand = commandBuf + (numCommands++);
				curCommand->cmd = c;
				curCommand->data = (uint8_t *)buf + bufWritePos;
				curCommand->datalen = 0;
				state = 1;
			} else if (charType == 2 || charType == 3) { // data first nibble
				nibble = (charType == 2) ? (c - '0') : (c - 'a' + 10);
				if (state == 1) { // first nibble
					curCommand->data[curCommand->datalen] = nibble << 4;
					curCommand->datalen++;
					bufWritePos++;
					state = 2;
				} else { // second nibble
					curCommand->data[curCommand->datalen - 1] |= nibble;
					state = 1;
				}
			}
		}
	}
}

int main(void) {
	//DDRC |= _BV(5);
	cli();
	PORTD |= _BV(1); // PD1, TxD
	PORTD &= ~_BV(0); // PD0, RxD

	UBRR0L = 103; // 9600 baud at 16MHz
	UBRR0H = 0;

	// enable receiver, enable transmitter
	// 8 data bits, 1 stop bit, no parity
	UCSR0B = 0b00011000;
	UCSR0C = 0b00000110;
	sei();
	writeStr("Ready.\r\n");

	while(1) {
		readCmdLine();
		runCommands();
	}
}

