#!makefile

LIBSRC := glyphs.c myx.c image.c
LIBOBJ := $(LIBSRC:.c=.o)

libmyx.a: $(LIBOBJ)
	ar -rc $@ $^

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -rf *.o libmyx.a


