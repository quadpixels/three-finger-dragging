#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# During distcheck, system locations (as provided by pkg-config) are
# not writable; provide instead relative locations.
DISTCHECK_CONFIGURE_FLAGS = \
	--with-sdkdir='$${includedir}/xorg' \
	--with-xorg-conf-dir='$${sysconfdir}/xorg'

MAINTAINERCLEANFILES = ChangeLog INSTALL

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xorg-synaptics.pc

sdk_HEADERS = include/synaptics.h include/synaptics-properties.h

input_LTLIBRARIES = @DRIVER_NAME@_drv.la

# -module lets us name the module exactly how we want
# -avoid-version prevents gratuitous .0.0.0 version numbers on the end
# -shared avoid building the static archive
@DRIVER_NAME@_drv_la_LDFLAGS = -module -avoid-version -shared
@DRIVER_NAME@_drv_la_CPPFLAGS = -I$(top_srcdir)/include
@DRIVER_NAME@_drv_la_CFLAGS = $(XORG_CFLAGS)

@DRIVER_NAME@_drv_la_SOURCES = src/@DRIVER_NAME@.c src/synapticsstr.h \
	src/synproto.h \
	src/properties.c

if BUILD_PS2COMM
@DRIVER_NAME@_drv_la_SOURCES += \
	src/alpscomm.c \
	src/ps2comm.c src/ps2comm.h
endif

if BUILD_EVENTCOMM
@DRIVER_NAME@_drv_la_SOURCES += \
	src/eventcomm.c src/eventcomm.h
endif

if BUILD_PSMCOMM
@DRIVER_NAME@_drv_la_SOURCES += \
	src/psmcomm.c
endif

bin_PROGRAMS = synclient syndaemon

synclient_SOURCES = tools/synclient.c
synclient_CFLAGS = -I$(top_srcdir)/include $(XORG_CFLAGS) $(XI_CFLAGS)
synclient_LDADD = $(XI_LIBS)

syndaemon_SOURCES = tools/syndaemon.c
syndaemon_CFLAGS = -I$(top_srcdir)/include $(XORG_CFLAGS) $(XI_CFLAGS) $(XTST_CFLAGS)
syndaemon_LDADD = $(XI_LIBS) $(XTST_LIBS)

if HAS_XORG_CONF_DIR
dist_config_DATA = conf/50-synaptics.conf
else
fdidir = $(datadir)/hal/fdi/policy/20thirdparty
dist_fdi_DATA = conf/11-x11-synaptics.fdi
endif

if ENABLE_UNIT_TESTS
if BUILD_EVENTCOMM
check_PROGRAMS = eventcomm-test

eventcomm_test_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/include
eventcomm_test_CFLAGS = $(XORG_CFLAGS) $(CWARNFLAGS)
eventcomm_test_SOURCES = test/eventcomm-test.c\
			 src/eventcomm.c \
			 test/fake-symbols.c test/fake-symbols.h

TESTS = $(check_PROGRAMS)
endif
endif

synclientmandir = $(APP_MAN_DIR)
synclientman_PRE = man/synclient.man
synclientman_DATA = $(synclientman_PRE:man=@APP_MAN_SUFFIX@)

syndaemonmandir = $(APP_MAN_DIR)
syndaemonman_PRE = man/syndaemon.man
syndaemonman_DATA = $(syndaemonman_PRE:man=@APP_MAN_SUFFIX@)

drivermandir = $(DRIVER_MAN_DIR)
driverman_PRE = man/@DRIVER_NAME@.man
driverman_DATA = $(driverman_PRE:man=@DRIVER_MAN_SUFFIX@)

EXTRA_DIST = man/@DRIVER_NAME@.man man/synclient.man man/syndaemon.man

CLEANFILES = $(driverman_DATA) $(synclientman_DATA) $(syndaemonman_DATA)

SUFFIXES = .$(DRIVER_MAN_SUFFIX) .man

.PHONY: ChangeLog INSTALL mandir

mandir:
	$(MKDIR_P) man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
.man.$(DRIVER_MAN_SUFFIX):
	@$(MKDIR_P) man
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

.man.$(APP_MAN_SUFFIX):
	@$(MKDIR_P) man
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

dist_doc_DATA = ChangeLog README docs/README.alps		\
		docs/trouble-shooting.txt docs/tapndrag.dia
