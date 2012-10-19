CC	= gcc
AS  = gcc
LD	= ld

CFLAGS	= -Wall -Wextra -nostdlib -fno-builtin -nostartfiles -nodefaultlibs \
          -I. 

OBJFILES = loader.o kernel.o console.o printfmt.o readline.o string.o printf.o

all: kernel.bin

%.o: %.S
	$(AS) -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

kernel.bin: $(OBJFILES)
	$(LD) -T linker.ld -o $@ $^ $(shell $(CC) -print-libgcc-file-name)

clean:
	$(RM) $(OBJFILES) kernel.bin
