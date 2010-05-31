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
        1.2.99,
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
        xf86-input-synaptics)

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

DRIVER_NAME=synaptics
AC_SUBST([DRIVER_NAME])

# Require xorg-macros: XORG_DEFAULT_OPTIONS
m4_ifndef([XORG_MACROS_VERSION], 
          [m4_fatal([must install xorg-macros 1.3 or later before running autoconf/autogen])])
XORG_MACROS_VERSION(1.3)
XORG_DEFAULT_OPTIONS

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC

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


# Obtain compiler/linker options for the Synaptics driver dependencies
PKG_CHECK_MODULES(XORG, xorg-server xproto)

# ----------------------------------------------------------------------------------
#			Dependencies for synclient and syndaemon
# ----------------------------------------------------------------------------------
AC_ARG_ENABLE([tools],
              AS_HELP_STRING([--enable-tools], [Build synclient and syndaemon [[default=auto]]]),
              [build_tools="$enableval"],
              [build_tools="auto"])

# Obtain compiler/linker options common for both synclient and syndaemon dependencies
PKG_CHECK_MODULES(XI, x11 inputproto [xi >= 1.2], have_depends="yes", have_depends="no")
if test "x$have_depends" = "xyes"; then
   if test "x$build_tools" != "xno"; then
      # The syndaemon program uses an optional XRecord extension implementation
      # If libxtst >= 1.0.99 is installed, Cflags contains the path to record.h
      PKG_CHECK_MODULES(XTST, xtst, have_libxtst="yes", have_libxtst="no")
      if test "x$have_libxtst" = "xyes" ; then
         # If recordproto < 1.13.99.1 is installed, Cflags contains the path to record.h
         PKG_CHECK_MODULES(RECORD_PROTO, recordproto, have_recordproto="yes", have_recordproto="no")
         # Header record.h may come from the xtst or recordproto package, or may be missing
         SAVE_CPPFLAGS="$CPPFLAGS"
         CPPFLAGS="$CPPFLAGS $XTST_CFLAGS $RECORD_PROTO_CFLAGS"
         AC_CHECK_HEADERS([X11/extensions/record.h],,,[#include <X11/Xlib.h>])
         CPPFLAGS="$SAVE_CPPFLAGS"
      fi
   fi
else
   if test "x$build_tools" = "xyes"; then
       AC_MSG_ERROR([Dependencies failed for synclient and syndaemon.])
   else
       AC_MSG_WARN([Not building synclient and syndaemon.])
   fi
fi
AM_CONDITIONAL(BUILD_TOOLS, [test x$have_depends = xyes && test x$build_tools != xno])

# ----------------------------------------------------------------------------------

sdkdir=`$PKG_CONFIG --variable=sdkdir xorg-server`
AC_SUBST([sdkdir])

AC_ARG_WITH(xorg-conf-dir,
            AC_HELP_STRING([--with-xorg-conf-dir=DIR],
                           [Default xorg.conf.d directory [[default=from $PKG_CONFIG xorg-server]]]),
            [XORG_CONF_DIR="$withval"],
            [XORG_CONF_DIR="`$PKG_CONFIG --variable=sysconfigdir xorg-server`"])
AC_SUBST(XORG_CONF_DIR)
AM_CONDITIONAL(HAS_XORG_CONF_DIR, [test "x$XORG_CONF_DIR" != "x" && test "x$XORG_CONF_DIR" != "xno"])

# Checks for header files.
AC_HEADER_STDC

AC_OUTPUT([Makefile
           src/Makefile
           man/Makefile
           tools/Makefile
           conf/Makefile
           include/Makefile
           xorg-synaptics.pc])
