#!makefile

TARGET := randr
SRC := main.c
OBJ := $(SRC:.c=.o)
LIBS := X11 Xrandr
LIBS := $(addprefix -l,$(LIBS))

all: $(TARGET).out

$(TARGET).out: $(OBJ)
	gcc $^ $(LIBS) -o $@

%.o: %.c
	gcc $(CFLAGS) -o $@ $<

install: $(TARGET).out
	cp $(TARGET).out $(INSTALLDIR)/$(TARGET)

clean:
	rm -rf *.o *.out


