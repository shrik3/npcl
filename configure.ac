dnl Process this file with autoconf to produce a configure script.
AC_INIT([libpcl], [1.12], [shrik3@mailbox.org])
AC_CONFIG_AUX_DIR([autosomething])

# sanity check
AC_CONFIG_SRCDIR([pcl.c])

# don't use automake for now
# AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# check for programs
AC_PROG_CC

AC_CONFIG_HEADERS([config.h])

# AC_CONFIG_MACRO_DIR([m4])
dnl Checks for programs.

# AC_PROG_RANLIB
# AC_PROG_INSTALL
# AC_PROG_LIBTOOL

dnl Checks for libraries.

dnl Checks for header files.
# AC_STDC_HEADERS

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_VOLATILE
AC_C_CONST
AC_C_BIGENDIAN

dnl Checks for library functions.
AC_CHECK_FUNCS(memset malloc free)
AC_CHECK_FUNCS(makecontext getcontext swapcontext)
AC_CHECK_FUNCS(sigaction)
AC_CHECK_FUNCS(longjmp setjmp)
AC_CHECK_FUNCS(sigaltstack)

dnl for now we don't use AC to generate makefiles
dnl AC_OUTPUT(Makefile pcl/Makefile test/Makefile man/Makefile include/Makefile)
AC_OUTPUT
