TOP = Xincludes/xc
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
MANDIR = /usr/local/man/man1

XF86SRC = $(SERVERSRC)/hw/xfree86
XF86COMSRC = $(XF86SRC)/common
XF86OSSRC = $(XF86SRC)/os-support


INCLUDES = -I. -I$(XF86COMSRC) -I$(SERVERSRC)/hw/xfree86/loader -I$(XF86OSSRC) -I$(SERVERSRC)/mi -I$(SERVERSRC)/include -I$(XINCLUDESRC) -I$(EXTINCSRC) -I$(TOP)/include -I$(SERVERSRC)/hw/xfree86 -I$(SERVERSRC)/hw/xfree86/parser -I$(XF86PCIINCLUDE)

ALLINCLUDES = $(INCLUDES) $(TOP_INCLUDES)

MODULE_DEFINES = -DIN_MODULE -DXFree86Module
PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO

STD_DEFINES = -Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE  -D_GNU_SOURCE  -DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP -DXCSECURITY -DTOGCUP   -DDPMSExtension  -DPIXPRIV -DPANORAMIX  -DRENDER -DGCCUSESGAS -DAVOID_GLYPHBLT -DPIXPRIV -DSINGLEDEPTH -DXFreeXDGA -DXvExtension -DXFree86LOADER  -DXFree86Server -DXF86VIDMODE  -DSMART_SCHEDULE -DBUILDDEBUG -DX_BYTE_ORDER=X_LITTLE_ENDIAN -DNDEBUG
ALLDEFINES = $(ALLINCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(MODULE_DEFINES)

check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CCOPTIONS = -pedantic -Wall -Wpointer-arith
CCOPTIONS += $(call check_gcc,-fno-merge-constants,)
CDEBUGFLAGS = -O2
CFLAGS = $(CDEBUGFLAGS) $(CCOPTIONS) $(ALLDEFINES)
CFLAGSCLIENT = $(CDEBUGFLAGS) $(CCOPTIONS) -I$(INSTALLED_X)/include

CC = gcc

LDCOMBINEFLAGS = -r

SRCS = synaptics.c ps2comm.c
OBJS = synaptics.o ps2comm.o

.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $(_NOOP_) $*.c

all:: synaptics_drv.o synclient syndaemon

install: $(BINDIR)/synclient $(BINDIR)/syndaemon $(INSTALLED_X)/lib/modules/input/synaptics_drv.o install-man

install-man: $(MANDIR)/synclient.1 $(MANDIR)/syndaemon.1

$(MANDIR)/synclient.1: manpages/synclient.1
	install -D $< $(DESTDIR)/$@

$(MANDIR)/syndaemon.1: manpages/syndaemon.1
	install -D $< $(DESTDIR)/$@

$(BINDIR)/synclient : synclient
	cp $< $@

$(BINDIR)/syndaemon : syndaemon
	cp $< $@

$(INSTALLED_X)/lib/modules/input/synaptics_drv.o : synaptics_drv.o
	cp $< $@

synaptics_drv.o: $(OBJS)
	$(RM) $@
	$(LD) $(LDCOMBINEFLAGS)  $(OBJS) -o $@

synclient.o	: synclient.c
	$(CC) $(CFLAGSCLIENT) -c -o $@ $<

synclient	: synclient.o
	$(CC) -o $@ $< -lm

syndaemon.o	: syndaemon.c
	$(CC) $(CFLAGSCLIENT) -c -o $@ $<

syndaemon	: syndaemon.o
	$(CC) -o $@ $< -lm -L$(INSTALLED_X)/lib -lXext -lX11

testprotokoll: testprotokoll.c
	$(CC) -o testprotokoll testprotokoll.c

synaptics.o : synaptics.h ps2comm.h linux_input.h
ps2comm.o   : ps2comm.h
synclient.o : synaptics.h
syndeamon.o : synaptics.h

clean::
	$(RM) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut synclient syndaemon "#"*

tags::
	etags -o TAGS *.c *.h
