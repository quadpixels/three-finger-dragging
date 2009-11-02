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
        1.2.0,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        xf86-input-synaptics)

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

DRIVER_NAME=synaptics
AC_SUBST([DRIVER_NAME])

# Require xorg-macros: XORG_DEFAULT_OPTIONS
m4_ifndef([XORG_MACROS_VERSION], [AC_FATAL([must install xorg-macros 1.3 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.3)
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
XORG_DEFAULT_OPTIONS

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

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug],
                                    [Enable debugging (default: disabled)]),
                                    [DEBUGGING=$enableval], [DEBUGGING=no])

if test "x$DEBUGGING" = xyes; then
       AC_DEFINE(DEBUG, 1, [Enable debugging code])
fi
AM_CONDITIONAL(DEBUG, [test "x$DEBUGGING" = xyes])

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

PKG_CHECK_MODULES(XORG16, [xorg-server >= 1.6],
                  HAVE_PROPERTIES="yes"; AC_DEFINE(HAVE_PROPERTIES, 1,
                                                   "Input Device Property support"),
                  HAVE_PROPERTIES="no");
AM_CONDITIONAL(HAVE_PROPERTIES, [ test "$HAVE_PROPERTIES" = "yes" ])
AC_SUBST(HAVE_PROPERTIES)

CFLAGS="$CFLAGS $XORG_CFLAGS "' -I$(top_srcdir)/src'
AC_SUBST([CFLAGS])

# Checks for libraries.
AC_ARG_ENABLE(tools,
              AC_HELP_STRING([--enable-tools], [Build synclient and syndaemon [[default=auto]]]),
              [build_tools="$enableval"],
              [build_tools="auto"])

if test "x$build_tools" != "xno"; then
    # Record.h is either in recordproto up to 1.13.2 or in libXtst 1.1.
    PKG_CHECK_MODULES(XTST_1_1, [xtst >= 1.0.99.1], HAVE_LIBXTST="yes", HAVE_LIBXTST="no")
    PKG_CHECK_MODULES(RECORDPROTO_1_13, xtst [recordproto < 1.13.99.1], HAVE_RECORDPROTO="yes", HAVE_RECORDPROTO="no")

    if test "x$HAVE_LIBXTST" = "xyes" || test "x$HAVE_RECORDPROTO" = "xyes" ; then
        AC_DEFINE([HAVE_XRECORD],[],[Use XRecord])
    fi

    PKG_CHECK_MODULES(XI, x11 [xi >= 1.2] [xorg-server >= 1.6],
                      BUILD_TOOLS="yes"; AC_DEFINE(BUILD_TOOLS, 1, "Build synclient and syndaemon"),
                      BUILD_TOOLS="no"; 
                      if test "x$build_tools" = "xyes"; then
                          AC_MSG_ERROR([Dependencies failed for synclient and syndaemon.])
                      else
                          AC_MSG_WARN([Not building synclient and syndaemon.])
                      fi
                      )
fi
AM_CONDITIONAL(BUILD_TOOLS, [ test "$BUILD_TOOLS" = "yes" ])
AC_SUBST(BUILD_TOOLS)

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
