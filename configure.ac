# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024 Tianhao Wang <i@shrik3.com>
#
# we only need autoconf the chec for several system function dependencies and
# produce a config header.
AC_INIT([libpcl], [1.12], [shrik3@mailbox.org])
AC_CONFIG_AUX_DIR([autosomething])
AC_CONFIG_SRCDIR([pcl.c])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CC
AC_C_INLINE
AC_C_VOLATILE
AC_C_CONST
AC_C_BIGENDIAN
AC_CHECK_FUNCS(memset malloc free)
AC_CHECK_FUNCS(makecontext getcontext swapcontext)
AC_CHECK_FUNCS(sigaction)
AC_CHECK_FUNCS(longjmp setjmp)
AC_CHECK_FUNCS(sigaltstack)
AC_OUTPUT
