TOP = /usr/src/redhat/BUILD/XFree86-4.2.0/xc
XTOP = $(TOP)
BUILDINCROOT = $(TOP)/exports
EXTINCSRC = $(XTOP)/include/extensions
INCLUDESRC = $(BUILDINCROOT)/include
XINCLUDESRC = $(INCLUDESRC)/X11
SERVERSRC = $(XTOP)/programs/Xserver
XF86PCIINCLUDE = $(TOP)/programs/Xserver/hw/xfree86/os-support/bus
TOP_X_INCLUDES = -I$(TOP)/exports/include
TOP_INCLUDES = -I$(TOP) $(TOP_X_INCLUDES)

INSTALLED_X = /usr/X11R6
BINDIR = /usr/local/bin

XF86SRC = $(SERVERSRC)/hw/xfree86
XF86COMSRC = $(XF86SRC)/common
XF86OSSRC = $(XF86SRC)/os-support


INCLUDES = -I. -I$(XF86COMSRC) -I$(SERVERSRC)/hw/xfree86/loader -I$(XF86OSSRC) -I$(SERVERSRC)/mi -I$(SERVERSRC)/include -I$(XINCLUDESRC) -I$(EXTINCSRC) -I$(TOP)/include -I$(SERVERSRC)/hw/xfree86 -I$(SERVERSRC)/hw/xfree86/parser -I$(XF86PCIINCLUDE)

ALLINCLUDES = $(INCLUDES) $(TOP_INCLUDES)

MODULE_DEFINES = -DIN_MODULE -DXFree86Module
PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO

STD_DEFINES = -Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE  -D_GNU_SOURCE  -DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP -DXCSECURITY -DTOGCUP   -DDPMSExtension  -DPIXPRIV -DPANORAMIX  -DRENDER -DGCCUSESGAS -DAVOID_GLYPHBLT -DPIXPRIV -DSINGLEDEPTH -DXFreeXDGA -DXvExtension -DXFree86LOADER  -DXFree86Server -DXF86VIDMODE  -DSMART_SCHEDULE -DBUILDDEBUG -DX_BYTE_ORDER=X_LITTLE_ENDIAN -DNDEBUG
ALLDEFINES = $(ALLINCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(MODULE_DEFINES)

CCOPTIONS = -pedantic -Wall -Wpointer-arith -fno-merge-constants
CDEBUGFLAGS = -O2
CFLAGS = $(CDEBUGFLAGS) $(CCOPTIONS) $(ALLDEFINES)
CFLAGSCLIENT = $(CDEBUGFLAGS) $(CCOPTIONS) -I$(INSTALLED_X)/include

CC = gcc

LDCOMBINEFLAGS = -r

SRCS = synaptics.c ps2comm.c
OBJS = synaptics.o ps2comm.o

DRIVER = synaptics

.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $(_NOOP_) $*.c

all:: $(DRIVER)_drv.o synclient

install: $(BINDIR)/synclient $(INSTALLED_X)/lib/modules/input/$(DRIVER)_drv.o

$(BINDIR)/synclient : synclient
	cp $< $@

$(INSTALLED_X)/lib/modules/input/$(DRIVER)_drv.o : $(DRIVER)_drv.o
	cp $< $@

$(DRIVER)_drv.o:  $(OBJS) $(EXTRALIBRARYDEPS)
	$(RM) $@
	$(LD) $(LDCOMBINEFLAGS)  $(OBJS) -o $@

synclient.o	: synclient.c
	$(CC) $(CFLAGSCLIENT) -c -o $@ $<

synclient	: synclient.o
	$(CC) -o $@ $< -lm

testprotokoll: testprotokoll.c
	$(CC) -o testprotokoll testprotokoll.c

synaptics.o : synaptics.h ps2comm.h
ps2comm.o   : ps2comm.h
synclient.o : synaptics.h

clean::
	$(RM) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut synclient "#"*

tags::
	$(TAGS) -w *.[ch]
	$(TAGS) -xw *.[ch] > TAGS




