CROSS_COMPILE	:= tc32-elf-
TEXT_START	:= 0x00000000

CC		:= $(CROSS_COMPILE)gcc
LD		:= $(CROSS_COMPILE)ld
OBJCOPY		:= $(CROSS_COMPILE)objcopy
OBJDUMP		:= $(CROSS_COMPILE)objdump

BIN_FLAGS	:= -O binary -R .reginfo -R .note -R .comment -R .mdebug -S

CFLAGS		:= -Wall -Wstrict-prototypes -Os -g --std=gnu99 \
		   -fstrict-aliasing -fno-pic \
		   -fdata-sections -ffunction-sections -pipe \
		   -fno-builtin-free

ASFLAGS		:= $(CFLAGS) -D__ASSEMBLY__

LDFLAGS		= -static -g --gc-sections
LDFLAGS		+= -T tlsr825x.ld -Map main.map -Ttext $(TEXT_START)

OBJECTS		:= startup825x.o main.o printf.o

all: main.bin main.s

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.s : %.c
	$(CC) $(CFLAGS) -S -g -fverbose-asm -o $@ $<

%.lst : %.c
	$(CC) $(CFLAGS) -c -g -Wa,-ahl=$@ $< > /dev/null

%.o : %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

main: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

main.bin: main
	$(OBJCOPY) $(BIN_FLAGS) $< $@

mrproper: clean

clean:
	rm -f main *.elf *.bin *.o *.s *~
