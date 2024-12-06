/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi <davidel@xmailserver.org>
 */

#if !defined(PCL_CONFIG_H)
#define PCL_CONFIG_H

#include "config.h"
#include <features.h>

// USER CONFIGURABLE:
// TODO: make this more configurable via script
// chose one of the coroutine implementations
// the longjmp version is faster but doesn't work with multi thread

// #define CO_USE_UCONTEXT
// #define CO_USE_LONGJMP
// #define CO_MULTI_THREAD


////////////////////// sanity checks ////////////////////////

#if !defined(__GLIBC__) || !defined(__GLIBC_MINOR__)
#error "what world do you live in"
#endif

// if by any chance the user we dno't define a mode, then use a default (w. MT)
#if !defined(CO_USE_UCONTEXT) && !defined(CO_USE_LONGJMP)
#warning "mode not specified, defaulting to CO_USE_LONGJMP w/o multithread"
#define CO_USE_LONGJMP
#endif // end setup default mode

// check system dependency for UCONTEXT mode
#if defined(CO_USE_UCONTEXT)
#if !defined(HAVE_GETCONTEXT) || !defined(HAVE_MAKECONTEXT) || !defined(HAVE_SWAPCONTEXT)
#error "your system doesn't provide getcontext/makecontext/swapcontext for UCONTEXT mode to work"
#endif // end check for system function deps
#endif // end ifdef CO_USE_UCONTEXT

#if defined(CO_USE_LONGJMP)
#if !defined(HAVE_SIGACTION) || !defined(HAVE_SIGALTSTACK)
#error "your system doesn't provide sigaction/sigaltstack for longjmp mode to work"
#endif // end check for system function deps
#define CO_USE_SIGCONTEXT
#endif //end ifdef CO_USE_LONGJMP


#endif // end include guard
