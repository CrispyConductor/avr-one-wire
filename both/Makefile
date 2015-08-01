MMCU = attiny44
PARTNO = t44
PROGRAMMER = avrispmkII
FREQ=8000000UL

all: main

one_wire_slave.o: one_wire_slave.c one_wire_slave.h one_wire_conf.h
	avr-gcc -DF_CPU=$(FREQ) -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c one_wire_slave.c

dallas_one_wire.o: dallas_one_wire.c dallas_one_wire.h one_wire_conf.h
	avr-gcc -DF_CPU=$(FREQ) -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c dallas_one_wire.c

main: one_wire_slave.o dallas_one_wire.o main.o
	avr-gcc -DF_CPU=$(FREQ) -mmcu=$(MMCU) -o main.elf main.o one_wire_slave.o dallas_one_wire.o
	avr-objcopy -O ihex main.elf main.hex

main.o: main.c one_wire_slave.h one_wire_conf.h
	avr-gcc -DF_CPU=$(FREQ) -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c main.c

program: main
	sudo avrdude -p $(PARTNO) -c $(PROGRAMMER) -U flash:w:./main.hex:i

one_wire_slave.s: one_wire_slave.o
	avr-gcc -DF_CPU=$(FREQ) -g -Os -Wall -std=c99 -mmcu=$(MMCU) -c -S one_wire_slave.c

clean:
	rm -f *.elf *.o *.hex *.s

