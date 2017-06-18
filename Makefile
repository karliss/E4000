
CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump

PRG = e4000
DEVICE  = atmega88pa
F_CPU = 12000000

USBDRV_D = external/v-usb/usbdrv
CFLAGS  = -Os -I${USBDRV_D} -Isrc -DDEBUG_LEVEL=0 -DF_CPU=$(F_CPU) -mmcu=$(DEVICE) -g -funsigned-char

SRC = src/main.cpp
OBJ = ${USBDRV_D}/usbdrv.o ${USBDRV_D}/usbdrvasm.o ${USBDRV_D}/oddebug.o ${SRC:.c=.o,.cpp=.o}

all: $(PRG).elf lst text

$(PRG).elf: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	avr-size $(PRG).elf

.S.o:
	$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

clean:
	rm -rf *.o $(PRG).elf $(OBJ)
	rm -rf *.lst *.map

lst:  $(PRG).lst
%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

text: hex bin
hex:  $(PRG).hex
bin:  $(PRG).bin

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@
%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@

