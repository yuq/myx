#!makefile

CONTEXT := GLX

DEFS :=
LIBS := -lGL

ifeq ($(CONTEXT),GLX)
DEFS += -DUSE_GLX
LIBS += -lX11
endif

ifeq ($(CONTEXT),EGL)
DEFS += -DUSE_EGL
LIBS += -lEGL
endif

ifeq ($(CONTEXT),GLUT)
DEFS += -DUSE_GLUT
LIBS += -lglut
endif

comp: main.c
	gcc -g $(DEFS) $< $(LIBS) -o $@

clean:
	rm -rf comp

