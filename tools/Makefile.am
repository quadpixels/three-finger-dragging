#  Copyright 2008 Red Hat, Inc.
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

if BUILD_TOOLS
bin_PROGRAMS = \
	synclient \
	syndaemon

INCLUDES=-I$(top_srcdir)/include/

synclient_SOURCES = synclient.c
synclient_LDFLAGS = -lm $(XI_LIBS)

syndaemon_SOURCES = syndaemon.c
syndaemon_LDFLAGS = $(XTST_1_1_LIBS) $(RECORDPROTO_1_13_LIBS) $(XI_LIBS)
else
all:
	@echo "Not building syndaemon and synclient."
endif
