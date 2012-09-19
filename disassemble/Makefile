#!makefile

DISASMSRC := asm.c dump.c main.c misc.c debug.c list.c
DISASMOBJ := $(DISASMSRC:.c=.o)

all: disasm

disasm: $(DISASMOBJ)
	gcc $^ -o $@

%.o: %.c
	gcc -I. -c $< -o $@

clean:
	rm -rf *.o disasm

