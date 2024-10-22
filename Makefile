VPATH = src
TARGET = megatext
# Common source files
ASM_SRCS =
C_SRCS = main.c memory.c error.c

# Object files
OBJS = $(ASM_SRCS:%.s=obj/%.o) $(C_SRCS:%.c=obj/%.o)
OBJS_DEBUG = $(ASM_SRCS:%.s=obj/%-debug.o) $(C_SRCS:%.c=obj/%-debug.o)
HEADERS = $(wildcard include/*.h)

.PHONY: all run
all: $(TARGET).prg $(TARGET).elf

obj/%.o: %.s
	as6502 --target=mega65 --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%.o: %.c $(HEADERS)
	cc6502 --target=mega65 -O2 -Iinclude --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%-debug.o: %.s
	as6502 --target=mega65 --debug --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%-debug.o: %.c
	cc6502 --target=mega65 --debug -Iinclude --list-file=$(@:%.o=%.lst) -o $@ $<

$(TARGET).prg: $(OBJS)
	ln6502 --target=mega65 --rtattr printf=nofloat mega65-plain.scm -o $@ $^  --output-format=prg --list-file=$(TARGET)-mega65.lst

$(TARGET).elf: $(OBJS_DEBUG)
	ln6502 --target=mega65 --rtattr printf=nofloat mega65-plain.scm --debug -o $@ $^ --list-file=$(TARGET)-debug.lst --semi-hosted

clean:
	-rm -f obj/*
	-rm *.prg *.lst

xemu: $(TARGET).prg
	xmega65 -besure -prgmode 65 -prg $(TARGET).prg

run: $(TARGET).prg
	etherload -r $(TARGET).prg