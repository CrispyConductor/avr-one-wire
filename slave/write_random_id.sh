#!/bin/bash

EEPROMFILE1=/tmp/avreeprom1.bin
EEPROMFILE2=/tmp/avreeprom2.bin

if [ $# -ne 3 ]; then
	echo "Usage: $0 <PartNo> <ProgrammerId> <EEPromAddress>"
	exit 1
fi

# Generate the ID
ID_PREFIX="`echo -en '\xf0\x0d'`"
ID="${ID_PREFIX}`head -c6 /dev/urandom`"
ID_HEX=`echo -n "$ID" | xxd -p`
echo "ID is: $ID_HEX"

# Read the current eeprom contents
sudo avrdude -p $1 -c $2 -U eeprom:r:$EEPROMFILE1:r

# Create the new eeprom contents
(
	head -c $3 $EEPROMFILE1
	echo -n "$ID"
	tail -c "+`expr $3 + 9`" $EEPROMFILE1
) > $EEPROMFILE2

# Sanity check
LEN1=`echo -n $EEPROMFILE1 | wc -c`
LEN2=`echo -n $EEPROMFILE2 | wc -c`
if [ $LEN1 != $LEN2 ]; then
	echo "Unexpected error.  Lengths do not match."
	exit 1
fi

# Write back to the avr
sudo avrdude -p $1 -c $2 -U eeprom:w:$EEPROMFILE2:r

echo "Done."
echo "ID is: $ID_HEX"

