VERSION=0.13.3

# Define the TOP variable to build using include files from a local source tree.
#TOP = /usr/src/redhat/BUILD/XFree86-4.3.0/xc

INSTALLED_X = $(DESTDIR)/usr/X11R6
LOCAL_X = Xincludes/usr/X11R6
BINDIR = $(DESTDIR)/usr/local/bin
MANDIR = $(DESTDIR)/usr/local/man/man1
LIBDIR = lib

ifeq ($(TOP),)
  # This hack attempts to check if the needed XFree86 header files are installed.
  # It checks for a needed XFree86 4.3.00 SDK header file that is not installed by
  # default. If it is present, then it assumes that all header files are present.
  # If it is not present, then it assumes that all header files are not present
  # and uses the local copy of the XFree86 4.2.0 header files.
  X_INCLUDES_ROOT = $(shell \
    if [ -f $(INSTALLED_X)/lib/Server/include/xisb.h ] ; then \
      echo -n $(INSTALLED_X) ; \
    else \
      echo -n $(LOCAL_X) ; \
    fi )
  ALLINCLUDES = -I. -I$(X_INCLUDES_ROOT)/include/X11 \
		-I$(X_INCLUDES_ROOT)/include/X11/extensions \
		-I$(X_INCLUDES_ROOT)/lib/Server/include
else
  SERVERSRC = $(TOP)/programs/Xserver
  ALLINCLUDES = -I. \
	-I$(SERVERSRC)/hw/xfree86/common \
	-I$(SERVERSRC)/hw/xfree86/os-support \
	-I$(SERVERSRC)/mi \
	-I$(SERVERSRC)/include \
	-I$(TOP)/include
  X_INCLUDES_ROOT = $(TOP)
endif

MODULE_DEFINES = -DIN_MODULE -DXFree86Module
PROTO_DEFINES = -DFUNCPROTO=15 -DNARROWPROTO

STD_DEFINES = -Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L -D_POSIX_SOURCE -D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE  -D_GNU_SOURCE  -DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP -DXCSECURITY -DTOGCUP   -DDPMSExtension  -DPIXPRIV -DPANORAMIX  -DRENDER -DGCCUSESGAS -DAVOID_GLYPHBLT -DPIXPRIV -DSINGLEDEPTH -DXFreeXDGA -DXvExtension -DXFree86LOADER  -DXFree86Server -DXF86VIDMODE  -DSMART_SCHEDULE -DBUILDDEBUG -DX_BYTE_ORDER=X_LITTLE_ENDIAN -DNDEBUG
ALLDEFINES = $(ALLINCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(MODULE_DEFINES)

check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

CCOPTIONS := -pedantic -Wall -Wpointer-arith
CCOPTIONS += $(call check_gcc,-fno-merge-constants,)
CCOPTIONS += $(call check_gcc,-fno-pic,)
CDEBUGFLAGS = -O2
CFLAGS = $(CDEBUGFLAGS) $(CCOPTIONS) $(ALLDEFINES) -DVERSION="\"$(VERSION)\""
CFLAGSCLIENT = $(CDEBUGFLAGS) $(CCOPTIONS) -DVERSION="\"$(VERSION)\"" -I$(X_INCLUDES_ROOT)/include

CC = gcc

LDCOMBINEFLAGS = -r

SRCS = synaptics.c ps2comm.c eventcomm.c psmcomm.c alpscomm.c
OBJS = synaptics.o ps2comm.o eventcomm.o psmcomm.o alpscomm.o

.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $(_NOOP_) $*.c

all:: synaptics_drv.o synclient syndaemon

install: $(BINDIR)/synclient $(BINDIR)/syndaemon $(INSTALLED_X)/$(LIBDIR)/modules/input/synaptics_drv.o install-man

install-man: $(MANDIR)/synclient.1 $(MANDIR)/syndaemon.1

$(MANDIR)/synclient.1: manpages/synclient.1
	install -D $< $@

$(MANDIR)/syndaemon.1: manpages/syndaemon.1
	install -D $< $@

$(BINDIR)/synclient : synclient
	install -D $< $@

$(BINDIR)/syndaemon : syndaemon
	install -D $< $@

$(INSTALLED_X)/$(LIBDIR)/modules/input/synaptics_drv.o : synaptics_drv.o
	install -D $< $@

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
	$(CC) -o $@ $< -lm -L$(INSTALLED_X)/$(LIBDIR) -lXext -lX11

synaptics.o : synaptics.h synproto.h Makefile
ps2comm.o   : ps2comm.h synproto.h synaptics.h
eventcomm.o : eventcomm.h linux_input.h synproto.h synaptics.h
psmcomm.o   : freebsd_mouse.h psmcomm.h synproto.h synaptics.h ps2comm.h
alpscomm.o  : alpscomm.h ps2comm.h synproto.h synaptics.h
synclient.o : synaptics.h Makefile
syndaemon.o : synaptics.h

clean::
	$(RM) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut synclient syndaemon "#"* manpages/*~ synaptics-$(VERSION).tar.bz2

tags::
	etags -o TAGS *.c *.h

uninstall::
	$(RM) $(BINDIR)/synclient $(BINDIR)/syndaemon $(INSTALLED_X)/lib/modules/input/synaptics_drv.o $(MANDIR)/synclient.1 $(MANDIR)/syndaemon.1

distribution : synaptics-$(VERSION).tar.bz2

ALLFILES = COMPATIBILITY FILES INSTALL INSTALL.DE INSTALL.FR LICENSE Makefile \
	NEWS README README.alps TODO Xincludes/ alps.patch linux_input.h \
	pc_keyb.c.diff.2.4.3 \
	synproto.h ps2comm.c ps2comm.h eventcomm.c eventcomm.h alpscomm.c alpscomm.h \
	psmcomm.c psmcomm.h freebsd_mouse.h \
	synaptics.c synaptics.h synaptics.spec \
	synclient.c syndaemon.c

DST=synaptics-$(VERSION)

synaptics-$(VERSION).tar.bz2 : FORCE
	rm -f $(DST).tar.bz2
	rm -rf $(DST)
	mkdir $(DST) $(DST)/manpages $(DST)/script $(DST)/test $(DST)/docs
	cp -a $(ALLFILES) $(DST)
	cp -a manpages/{synclient.1,syndaemon.1} $(DST)/manpages/
	cp -a script/usbmouse $(DST)/script/
	cp -a test/{test-pad.c,testprotocol.c} $(DST)/test/
	cp -a docs/tapndrag.dia $(DST)/docs/
	chmod u+w $(DST)/*
	tar cf $(DST).tar $(DST)
	rm -rf $(DST)
	bzip2 $(DST).tar

.PHONY: FORCE
