#!makefile

TARGET := wm
SRC := main.c
OBJ := $(SRC:.c=.o)
LIBS := X11 Xrender Xcomposite Xdamage Xfixes
LIBS := $(addprefix -l,$(LIBS))

all: $(TARGET).out

$(TARGET).out: $(OBJ)
	g++ $^ $(LIBS) -o $@

%.o: %.cpp
	g++ $(CFLAGS) -o $@ $<

install: $(TARGET).out
	cp $(TARGET).out $(INSTALLDIR)/$(TARGET)

clean:
	rm -rf *.o *.out


