/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(PCL_PRIVATE_H)
#define PCL_PRIVATE_H

#include <stdio.h>
#include <stdlib.h>
#include "../pcl_config.h"
#include "pcl.h"

#if defined(CO_USE_UCONTEXT)
#include <ucontext.h>

typedef ucontext_t co_core_ctx_t;
#else
#include <setjmp.h>

typedef jmp_buf co_core_ctx_t;
#endif

/*
 * The following value must be power of two (N^2).
 */
#define CO_STK_ALIGN 256
#define CO_STK_COROSIZE ((sizeof(coroutine) + CO_STK_ALIGN - 1) & ~(CO_STK_ALIGN - 1))
#define CO_MIN_SIZE (4 * 1024)

typedef struct s_co_ctx {
	co_core_ctx_t cc;
} co_ctx_t;

typedef struct s_coroutine {
	co_ctx_t ctx;
	int alloc;
	struct s_coroutine *caller;
	struct s_coroutine *restarget;
	void (*func)(void *);
	void *data;
} coroutine;

typedef struct s_cothread_ctx {
	coroutine co_main;
	coroutine *co_curr;
	coroutine *co_dhelper;
	coroutine *dchelper;
	char stk[CO_MIN_SIZE];
} cothread_ctx;

cothread_ctx *co_get_thread_ctx(void);
#endif

