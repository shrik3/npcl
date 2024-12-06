/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi <davidel@xmailserver.org>
 *  Copyright (C) 2024 Tianhao Wang <i@shrik3.com>
 *
 *  is very luckily broken, don't use this !!!!!
 *
 *  this is an experimental single header implementation version. it may work
 *  but is not strictly tested. There may be some problems with global variables:
 *  in the pre-compiled pcl there is only one instance of each global variables.
 *  However as a single header implementation, each .c file that include this
 *  will wither 1) have a duplicate instance of the global if declared static or
 *  2) have a re-definition compiler error
 *
 *  note that most comments are stripped for a cleaner file, in doubt, refer to
 *  their original source code.
 */

#if !defined(NPCL_H)
#define NPCL_H

// you need to bring your configurations, checkout makefile and configure.ac
#define CO_USE_LONGJMP
// #define CO_USE_UCONTEXT
// #define CO_MULTI_THREAD

// if by any chance the user we dno't define a mode, then use a default (w. MT)
#if !defined(CO_USE_UCONTEXT) && !defined(CO_USE_LONGJMP)
#warning "mode not specified, defaulting to CO_USE_LONGJMP w/o multithread"
#define CO_USE_LONGJMP
#endif // end setup default mode

// check system dependency for UCONTEXT mode
#if defined(CO_USE_UCONTEXT)
#endif // end ifdef CO_USE_UCONTEXT

#if defined(CO_USE_LONGJMP)
#define CO_USE_SIGCONTEXT
#endif

#if defined(CO_USE_SIGCONTEXT) && defined(CO_MULTI_THREAD)
#error "Sorry, it is not possible to use MT libpcl with CO_USE_SIGCONTEXT"
#endif

// APIs ///////////////////////////////////////////////////////////////
typedef void *coroutine_t;
int co_thread_init(void);
void co_thread_cleanup(void);
coroutine_t co_create(void (*func)(void *), void *data, void *stack, int size);
void co_delete(coroutine_t coro);
void co_call(coroutine_t coro);
void co_resume(void);
void co_exit_to(coroutine_t coro);
void co_exit(void);
coroutine_t co_current(void);
void *co_get_data(coroutine_t coro);
void *co_set_data(coroutine_t coro, void *data);

// pcl_private.h //////////////////////////////////////////////////////
#include "pcl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// we are certainly using glibc/linux, here simply checking versions
#include <features.h>

#if defined(CO_USE_UCONTEXT)
#include <ucontext.h>
typedef ucontext_t co_core_ctx_t;
#else
#include <setjmp.h>
#include <signal.h>
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

// pcl_private.c //////////////////////////////////////////////////////
static cothread_ctx *co_get_global_ctx(void)
{
	static cothread_ctx tctx;
	if (tctx.co_curr == NULL) {
		tctx.co_curr = &tctx.co_main;
	}
	return &tctx;
}

#if !defined(CO_MULTI_THREAD)
// Simple case, the single thread one ...
int co_thread_init(void) { return 0; }
void co_thread_cleanup(void) {}
cothread_ctx *co_get_thread_ctx(void) { return co_get_global_ctx(); }
#else
// MultiThread cases ...
// On Unix, we use pthread. Sligthly more complicated ...
#include <pthread.h>

static int valid_key;
static pthread_key_t key;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

static void co_once_init(void)
{
	if (pthread_key_create(&key, free))
		perror("creating TLS key");
	else
		valid_key++;
}

static int co_thread_init(void)
{
	cothread_ctx *tctx;

	pthread_once(&once_control, co_once_init);
	if (!valid_key) {
		return -1;
	}

	if ((tctx = (cothread_ctx *)malloc(sizeof(cothread_ctx))) == NULL) {
		perror("allocating context");
		return -1;
	}
	memset(tctx, 0, sizeof(*tctx));
	tctx->co_curr = &tctx->co_main;
	if (pthread_setspecific(key, tctx)) {
		perror("setting thread context");
		free(tctx);
		return -1;
	}

	return 0;
}

static void co_thread_cleanup(void) {}

static cothread_ctx *co_get_thread_ctx(void)
{
	cothread_ctx *tctx = (cothread_ctx *)(valid_key ? pthread_getspecific(key) : NULL);
	return tctx != NULL ? tctx : co_get_global_ctx();
}
#endif

// implementations (pcl.c) ////////////////////////////////////////////
#if defined(CO_USE_SIGCONTEXT)
static volatile int ctx_called;
static co_ctx_t *ctx_creating;
static void *ctx_creating_func;
static sigset_t ctx_creating_sigs;
static co_ctx_t ctx_trampoline;
static co_ctx_t ctx_caller;
#endif /* #if defined(CO_USE_SIGCONTEXT) */

static int co_ctx_sdir(unsigned long psp)
{
	int nav = 0;
	unsigned long csp = (unsigned long)&nav;
	return psp > csp ? -1 : +1;
}

static int co_ctx_stackdir(void)
{
	int cav = 0;
	return co_ctx_sdir((unsigned long)&cav);
}

#if defined(CO_USE_UCONTEXT)
static int co_set_context(co_ctx_t *ctx, void *func, char *stkbase, long stksiz)
{
	if (getcontext(&ctx->cc))
		return -1;

	ctx->cc.uc_link = NULL;
	ctx->cc.uc_stack.ss_sp = stkbase;
	ctx->cc.uc_stack.ss_size = stksiz - sizeof(long);
	ctx->cc.uc_stack.ss_flags = 0;
	makecontext(&ctx->cc, func, 1);
	return 0;
}

static void co_switch_context(co_ctx_t *octx, co_ctx_t *nctx)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	if (swapcontext(&octx->cc, &nctx->cc) < 0) {
		fprintf(stderr, "[PCL] Context switch failed: curr=%p\n", tctx->co_curr);
		exit(1);
	}
}

#else /* #if defined(CO_USE_UCONTEXT) */

#if defined(CO_USE_SIGCONTEXT)
static void co_ctx_bootstrap(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	co_ctx_t *volatile ctx_starting;
	void (*volatile ctx_starting_func)(void);
	sigprocmask(SIG_SETMASK, &ctx_creating_sigs, NULL);
	ctx_starting = ctx_creating;
	ctx_starting_func = (void (*)(void))ctx_creating_func;
	if (!setjmp(ctx_starting->cc))
		longjmp(ctx_caller.cc, 1);

	ctx_starting_func();
	fprintf(stderr, "[PCL] Hmm, you really shouldn't reach this point: curr=%p\n", tctx->co_curr);
	exit(1);
}

static void co_ctx_trampoline(int sig)
{
	if (setjmp(ctx_trampoline.cc) == 0) {
		ctx_called = 1;
		return;
	}
	co_ctx_bootstrap();
}

static int co_set_context(co_ctx_t *ctx, void *func, char *stkbase, long stksiz)
{
	struct sigaction sa;
	struct sigaction osa;
	sigset_t osigs;
	sigset_t sigs;
	stack_t ss;
	stack_t oss;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
	sa.sa_handler = co_ctx_trampoline;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if (sigaction(SIGUSR1, &sa, &osa) != 0)
		return -1;

	ss.ss_sp = stkbase;
	ss.ss_size = stksiz - sizeof(long);
	ss.ss_flags = 0;
	if (sigaltstack(&ss, &oss) < 0)
		return -1;

	ctx_called = 0;
	kill(getpid(), SIGUSR1);
	sigfillset(&sigs);
	sigdelset(&sigs, SIGUSR1);
	while (!ctx_called)
		sigsuspend(&sigs);

	sigaltstack(NULL, &ss);
	ss.ss_flags = SS_DISABLE;
	if (sigaltstack(&ss, NULL) < 0)
		return -1;
	sigaltstack(NULL, &ss);
	if (!(ss.ss_flags & SS_DISABLE))
		return -1;
	if (!(oss.ss_flags & SS_DISABLE))
		sigaltstack(&oss, NULL);

	sigaction(SIGUSR1, &osa, NULL);
	sigprocmask(SIG_SETMASK, &osigs, NULL);

	ctx_creating = ctx;
	ctx_creating_func = func;
	memcpy(&ctx_creating_sigs, &osigs, sizeof(sigset_t));

	if (setjmp(ctx_caller.cc) == 0)
		longjmp(ctx_trampoline.cc, 1);

	return 0;
}
#endif /* #if defined(CO_USE_SIGCONTEXT) */

static void co_switch_context(co_ctx_t *octx, co_ctx_t *nctx)
{
	if (setjmp(octx->cc) == 0)
		longjmp(nctx->cc, 1);
}

#endif /* #if defined(CO_USE_UCONTEXT) */

static void co_runner(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = tctx->co_curr;

	co->restarget = co->caller;
	co->func(co->data);
	co_exit();
}

coroutine_t co_create(void (*func)(void *), void *data, void *stack, int size)
{
	int alloc = 0, r = CO_STK_COROSIZE;
	coroutine *co;

	if ((size &= ~(sizeof(long) - 1)) < CO_MIN_SIZE)
		return NULL;
	if (stack == NULL) {
		size = (size + sizeof(coroutine) + CO_STK_ALIGN - 1) & ~(CO_STK_ALIGN - 1);
		stack = malloc(size);
		if (stack == NULL)
			return NULL;
		alloc = size;
	}
	co = stack;
	stack = (char *)stack + CO_STK_COROSIZE;
	co->alloc = alloc;
	co->func = func;
	co->data = data;
	if (co_set_context(&co->ctx, co_runner, stack, size - CO_STK_COROSIZE) < 0) {
		if (alloc)
			free(co);
		return NULL;
	}

	return (coroutine_t)co;
}

void co_delete(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *)coro;

	if (co == tctx->co_curr) {
		fprintf(stderr, "[PCL] Cannot delete itself: curr=%p\n", tctx->co_curr);
		exit(1);
	}
	if (co->alloc)
		free(co);
}

void co_call(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *)coro, *oldco = tctx->co_curr;
	co->caller = tctx->co_curr;
	tctx->co_curr = co;
	co_switch_context(&oldco->ctx, &co->ctx);
}

void co_resume(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();

	co_call(tctx->co_curr->restarget);
	tctx->co_curr->restarget = tctx->co_curr->caller;
}

static void co_del_helper(void *data)
{
	cothread_ctx *tctx;
	coroutine *cdh;
	for (;;) {
		tctx = co_get_thread_ctx();
		cdh = tctx->co_dhelper;
		tctx->co_dhelper = NULL;
		co_delete(tctx->co_curr->caller);
		co_call((coroutine_t)cdh);
		if (tctx->co_dhelper == NULL) {
			fprintf(stderr, "[PCL] Resume to delete helper coroutine: curr=%p caller=%p\n",
			        tctx->co_curr, tctx->co_curr->caller);
			exit(1);
		}
	}
}

void co_exit_to(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *)coro;

	if (tctx->dchelper == NULL &&
	    (tctx->dchelper = co_create(co_del_helper, NULL, tctx->stk, sizeof(tctx->stk))) == NULL) {
		fprintf(stderr, "[PCL] Unable to create delete helper coroutine: curr=%p\n", tctx->co_curr);
		exit(1);
	}
	tctx->co_dhelper = co;
	co_call((coroutine_t)tctx->dchelper);
	fprintf(stderr, "[PCL] Stale coroutine called: curr=%p  exitto=%p  caller=%p\n", tctx->co_curr,
	        co, tctx->co_curr->caller);
	exit(1);
}

void co_exit(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	co_exit_to((coroutine_t)tctx->co_curr->restarget);
}

coroutine_t co_current(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	return (coroutine_t)tctx->co_curr;
}

void *co_get_data(coroutine_t coro)
{
	coroutine *co = (coroutine *)coro;
	return co->data;
}

void *co_set_data(coroutine_t coro, void *data)
{
	coroutine *co = (coroutine *)coro;
	void *odata;
	odata = co->data;
	co->data = data;
	return odata;
}

#endif // include guard
