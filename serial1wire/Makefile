MMCU = atmega328p
PARTNO = m328p
PROGRAMMER = avrispmkII

all: serial1wire

dallas_one_wire.o: dallas_one_wire.c dallas_one_wire.h
	avr-gcc -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c dallas_one_wire.c

serial1wire: serial1wire.o dallas_one_wire.o
	avr-gcc -mmcu=$(MMCU) -o serial1wire.elf serial1wire.o dallas_one_wire.o -Wl,-u,vfprintf -lprintf_flt -lm
	avr-objcopy -O ihex serial1wire.elf serial1wire.hex

serial1wire.s: serial1wire.c
	avr-gcc -g -Os -Wall -std=c99 -mmcu=atmega328p -c -S serial1wire.c

serial1wire.o: serial1wire.c dallas_one_wire.h
	avr-gcc -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c serial1wire.c -o serial1wire.o

program: serial1wire
	sudo avrdude -p $(PARTNO) -c $(PROGRAMMER) -U flash:w:./serial1wire.hex:i

clean:
	rm -f *.elf *.o *.hex *.s

