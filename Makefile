VPATH = src

# Common source files
ASM_SRCS =
C_SRCS = main.c memory.c

# Object files
OBJS = $(ASM_SRCS:%.s=obj/%.o) $(C_SRCS:%.c=obj/%.o)
OBJS_DEBUG = $(ASM_SRCS:%.s=obj/%-debug.o) $(C_SRCS:%.c=obj/%-debug.o)
HEADERS = $(wildcard include/*.h)

.PHONY: all
all: hello.prg hello.elf

obj/%.o: %.s
	as6502 --target=mega65 --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%.o: %.c $(HEADERS)
	cc6502 --target=mega65 -O2 -Iinclude --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%-debug.o: %.s
	as6502 --target=mega65 --debug --list-file=$(@:%.o=%.lst) -o $@ $<

obj/%-debug.o: %.c
	cc6502 --target=mega65 --debug -Iinclude --list-file=$(@:%.o=%.lst) -o $@ $<

hello.prg: $(OBJS)
	ln6502 --target=mega65 --rtattr printf=nofloat mega65-plain.scm -o $@ $^  --output-format=prg --list-file=hello-mega65.lst

hello.elf: $(OBJS_DEBUG)
	ln6502 --target=mega65 --rtattr printf=nofloat mega65-plain.scm --debug -o $@ $^ --list-file=hello-debug.lst --semi-hosted

clean:
	-rm -f obj/*
	-rm *.prg *.lst

run: hello.prg
	xmega65 -besure -prgmode 65 -prg hello.prg
