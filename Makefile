#!makefile

APPS := hello wm randr

###################################################################

# $(call add_app,<appdir>,<target>)
#
# Add a app dir
#
define add_app
$2$1:
	$$(MAKE) -C $1 $2
endef

# $(call add_target,<target>)
#
# Add a target
#
define add_target
$1: $$(addprefix $1,$$(APPS))
$$(foreach app,$$(APPS),$$(eval $$(call add_app,$$(app),$1)))
endef

###################################################################

TOPDIR := $(shell pwd)
LIBDIR := $(TOPDIR)/lib/
INSTALLDIR := $(TOPDIR)/out/
INCLUDEDIR := $(TOPDIR)/include/

export TOPDIR INSTALLDIR

###################################################################

EXTERNLIBS := xrender freetype2

CFLAGS := -pipe -O2 -c -g -I$(INCLUDEDIR) `pkg-config --cflags $(EXTERNLIBS)`
LDFLAGS := -L$(LIBDIR) -lmyx `pkg-config --libs $(EXTERNLIBS)`

export CFLAGS LDFLAGS

###################################################################

all: libs

install: mkinstalldir

mkinstalldir:
	mkdir $(INSTALLDIR)

$(eval $(call add_target,all))
$(eval $(call add_target,install))
$(eval $(call add_target,clean))

clean: cleanlibs
	rm -rf $(INSTALLDIR)

libs:
	$(MAKE) -C $(LIBDIR)

cleanlibs:
	$(MAKE) -C $(LIBDIR) clean












