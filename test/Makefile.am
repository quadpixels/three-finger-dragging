AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = $(XORG_CFLAGS) $(CWARNFLAGS)
fake_syms = fake-symbols.c fake-symbols.h

if BUILD_EVENTCOMM
noinst_PROGRAMS = eventcomm-test

eventcomm_test_SOURCES = eventcomm-test.c\
			 $(top_srcdir)/src/eventcomm.c \
			 $(fake_syms)
endif

if ENABLE_UNIT_TESTS
TESTS = $(noinst_PROGRAMS)
endif
