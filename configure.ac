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
#
# Process this file with autoconf to produce a configure script

AC_PREREQ(2.57)
AC_INIT([xf86-input-synaptics],
        1.0.99.1,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        xf86-input-synaptics)

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

DRIVER_NAME=synaptics
AC_SUBST([DRIVER_NAME])

AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC

AH_TOP([#include "xorg-server.h"])

AC_MSG_CHECKING([which optional backends will be build])
case "${host}" in
*linux*)
	AC_MSG_RESULT([eventcomm])
	BUILD_EVENTCOMM="yes"
	BUILD_PSMCOMM="no"
	;;
*freebsd* | *openbsd* | *netbsd* | *dragonfly*)
	AC_MSG_RESULT([psmcomm])
	BUILD_EVENTCOMM="no"
	BUILD_PSMCOMM="yes"
	;;
*)
	AC_MSG_RESULT([none])
	BUILD_EVENTCOMM="no"
	BUILD_PSMCOMM="no"
	;;
esac
AM_CONDITIONAL([BUILD_EVENTCOMM], [test "x${BUILD_EVENTCOMM}" = "xyes"])
if test "x${BUILD_EVENTCOMM}" = "xyes" ; then
	AC_DEFINE(BUILD_EVENTCOMM, 1, [Optional backend eventcomm enabled])
fi
AM_CONDITIONAL([BUILD_PSMCOMM], [test "x${BUILD_PSMCOMM}" = "xyes"])
if test "x${BUILD_PSMCOMM}" = "xyes" ; then
	AC_DEFINE(BUILD_PSMCOMM, 1, [Optional backend psmcomm enabled])
fi

AC_ARG_WITH(xorg-module-dir,
            AC_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
            [moduledir="$withval"],
            [moduledir="$libdir/xorg/modules"])
inputdir=${moduledir}/input
AC_SUBST(inputdir)

# Checks for extensions
XORG_DRIVER_CHECK_EXT(XINPUT, inputproto)

# Checks for pkg-config packages
PKG_CHECK_MODULES(XORG, xorg-server xproto $REQUIRED_MODULES)
sdkdir=$(pkg-config --variable=sdkdir xorg-server)
AC_SUBST([sdkdir])

CFLAGS="$CFLAGS $XORG_CFLAGS "' -I$(top_srcdir)/src'
AC_SUBST([CFLAGS])

# Checks for libraries.
PKG_CHECK_MODULES(XLIB, x11) # needed for syndaemon
PKG_CHECK_MODULES(XRECORD, xtst, AC_DEFINE([HAVE_XRECORD],[],[Use XRecord]), true)
PKG_CHECK_MODULES(XI, xi);

# Checks for header files.
AC_HEADER_STDC

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
           src/Makefile
           man/Makefile
           tools/Makefile
           fdi/Makefile
           include/Makefile
           xorg-synaptics.pc])
