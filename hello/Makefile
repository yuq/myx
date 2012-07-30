#!makefile

TARGET := xrhello
SRC := xrhello.c
OBJ := $(SRC:.c=.o)
LIBS := freeimage
LDFLAGS += $(addprefix -l,$(LIBS))

all: $(TARGET).out

$(TARGET).out: $(OBJ)
	gcc $^ $(LDFLAGS) -o $@

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

install:
	cp $(TARGET).out $(INSTALLDIR)/$(TARGET)

clean:
	rm -rf *.o *.out
