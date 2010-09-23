#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.63)
AC_INIT([xburst-tools], [0.1])
AC_CONFIG_AUX_DIR(m4)
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

AC_ARG_ENABLE([firmware],
	[AS_HELP_STRING([--disable-firmware],
		[Do not build initialization code]
		[(for example, because some other package contains it).])],
	[AS_CASE([$enableval], [yes], [firmware=true], [no], [firmware=false],
		[AC_MSG_ERROR([bad value $enableval for --enable-firmware])])],
	[firmware=true])
AM_CONDITIONAL([COND_FIRMWARE], [test "$firmware" = true])

AC_ARG_VAR([CROSS_COMPILE],
	[Cross compiler prefix for building firmware, ]
	[e.g. CROSS_COMPILE=mipsel-openwrt-linux-.])

AS_VAR_IF([firmware], [true],
	[
	AS_VAR_IF([CROSS_COMPILE], [],
		[AC_MSG_ERROR([CROSS_COMPILE prefix is not set, set to cross-prefix- use --disable-firmware])])
	AC_CHECK_PROG([CROSS_GCC], [${CROSS_COMPILE}gcc], [yes], [no])
	AS_VAR_IF([CROSS_GCC], [no],
		[AC_MSG_ERROR([No cross compiler, check CROSS_COMPILE value])])
	],
	[CROSS_COMPILE="dummy-"]
)

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB([c], [main])
AC_CHECK_LIB([confuse], [main], [], [
echo "Error! You need to have libconfuse."
echo "Maybe run 'sudo apt-get install libconfuse-dev' under debian"
exit -1 ])
AC_CHECK_LIB([gcc], [main])
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([usb], [main], [], [
echo "Error! You need to have libusb. \n"
echo "Maybe run 'sudo apt-get install libusb-dev' under debian"
exit -1 ])

LIBS="$LIBS $USB_LIBS"
CFLAGS="$CFLAGS $USB_CFLAGS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([memset strerror])

AC_CONFIG_FILES(Makefile \
		usbboot/Makefile usbboot/src/Makefile \
		xbboot/Makefile xbboot/host-app/Makefile)
AC_OUTPUT
