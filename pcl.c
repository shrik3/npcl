/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi <davidel@xmailserver.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include "pcl_config.h"
#include "pcl.h"
#include "private/pcl_private.h"

// we are certainly using glibc/linux, here simply checking versions
#include <features.h>

#if defined(CO_USE_SIGCONTEXT)
#include <signal.h>
#include <unistd.h>
// alas, memcpy is provided by string.h
#include <string.h>
#endif

#if defined(CO_USE_SIGCONTEXT)

/*
 * Multi thread and CO_USE_SIGCONTEXT are a NO GO!
 */
#if defined(CO_MULTI_THREAD)
#error "Sorry, it is not possible to use MT libpcl with CO_USE_SIGCONTEXT"
#endif

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
	unsigned long csp = (unsigned long) &nav;

	return psp > csp ? -1: +1;
}

static int co_ctx_stackdir(void)
{
	int cav = 0;

	return co_ctx_sdir((unsigned long) &cav);
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
		fprintf(stderr, "[PCL] Context switch failed: curr=%p\n",
			tctx->co_curr);
		exit(1);
	}
}

#else /* #if defined(CO_USE_UCONTEXT) */

#if defined(CO_USE_SIGCONTEXT)

/*
 * This code comes from the GNU Pth implementation and uses the
 * sigstack/sigaltstack() trick.
 *
 * The ingenious fact is that this variant runs really on _all_ POSIX
 * compliant systems without special platform kludges.  But be _VERY_
 * carefully when you change something in the following code. The slightest
 * change or reordering can lead to horribly broken code.  Really every
 * function call in the following case is intended to be how it is, doubt
 * me...
 *
 * For more details we strongly recommend you to read the companion
 * paper ``Portable Multithreading -- The Signal Stack Trick for
 * User-Space Thread Creation'' from Ralf S. Engelschall.
 */
static void co_ctx_bootstrap(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	co_ctx_t * volatile ctx_starting;
	void (* volatile ctx_starting_func)(void);

	/*
	 * Switch to the final signal mask (inherited from parent)
	 */
	sigprocmask(SIG_SETMASK, &ctx_creating_sigs, NULL);

	/*
	 * Move startup details from static storage to local auto
	 * variables which is necessary because it has to survive in
	 * a local context until the thread is scheduled for real.
	 */
	ctx_starting = ctx_creating;
	ctx_starting_func = (void (*)(void)) ctx_creating_func;

	/*
	 * Save current machine state (on new stack) and
	 * go back to caller until we're scheduled for real...
	 */
	if (!setjmp(ctx_starting->cc))
		longjmp(ctx_caller.cc, 1);

	/*
	 * The new thread is now running: GREAT!
	 * Now we just invoke its init function....
	 */
	ctx_starting_func();

	fprintf(stderr, "[PCL] Hmm, you really shouldn't reach this point: curr=%p\n",
		tctx->co_curr);
	exit(1);
}

static void co_ctx_trampoline(int sig)
{
	/*
	 * Save current machine state and _immediately_ go back with
	 * a standard "return" (to stop the signal handler situation)
	 * to let him remove the stack again. Notice that we really
	 * have do a normal "return" here, or the OS would consider
	 * the thread to be running on a signal stack which isn't
	 * good (for instance it wouldn't allow us to spawn a thread
	 * from within a thread, etc.)
	 */
	if (setjmp(ctx_trampoline.cc) == 0) {
		ctx_called = 1;
		return;
	}

	/*
	 * Ok, the caller has longjmp'ed back to us, so now prepare
	 * us for the real machine state switching. We have to jump
	 * into another function here to get a new stack context for
	 * the auto variables (which have to be auto-variables
	 * because the start of the thread happens later).
	 */
	co_ctx_bootstrap();
}

static int co_set_context(co_ctx_t *ctx, void *func, char *stkbase, long stksiz)
{
	struct sigaction sa;
	struct sigaction osa;
	sigset_t osigs;
	sigset_t sigs;
	// I don't know how different they are but struct sigaltstack is compile
	// error
	// struct sigaltstack ss;
	// struct sigaltstack oss;
	stack_t ss;
	stack_t oss;

	/*
	 * Preserve the SIGUSR1 signal state, block SIGUSR1,
	 * and establish our signal handler. The signal will
	 * later transfer control onto the signal stack.
	 */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sigs, &osigs);
	sa.sa_handler = co_ctx_trampoline;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_ONSTACK;
	if (sigaction(SIGUSR1, &sa, &osa) != 0)
		return -1;

	/*
	 * Set the new stack.
	 *
	 * For sigaltstack we're lucky [from sigaltstack(2) on
	 * FreeBSD 3.1]: ``Signal stacks are automatically adjusted
	 * for the direction of stack growth and alignment
	 * requirements''
	 *
	 * For sigstack we have to decide ourself [from sigstack(2)
	 * on Solaris 2.6]: ``The direction of stack growth is not
	 * indicated in the historical definition of struct sigstack.
	 * The only way to portably establish a stack pointer is for
	 * the application to determine stack growth direction.''
	 */
	ss.ss_sp = stkbase;
	ss.ss_size = stksiz - sizeof(long);
	ss.ss_flags = 0;
	if (sigaltstack(&ss, &oss) < 0)
		return -1;

	/*
	 * Now transfer control onto the signal stack and set it up.
	 * It will return immediately via "return" after the setjmp()
	 * was performed. Be careful here with race conditions.  The
	 * signal can be delivered the first time sigsuspend() is
	 * called.
	 */
	ctx_called = 0;
	kill(getpid(), SIGUSR1);
	sigfillset(&sigs);
	sigdelset(&sigs, SIGUSR1);
	while (!ctx_called)
		sigsuspend(&sigs);

	/*
	 * Inform the system that we are back off the signal stack by
	 * removing the alternative signal stack. Be careful here: It
	 * first has to be disabled, before it can be removed.
	 */
	sigaltstack(NULL, &ss);
	ss.ss_flags = SS_DISABLE;
	if (sigaltstack(&ss, NULL) < 0)
		return -1;
	sigaltstack(NULL, &ss);
	if (!(ss.ss_flags & SS_DISABLE))
		return -1;
	if (!(oss.ss_flags & SS_DISABLE))
		sigaltstack(&oss, NULL);

	/*
	 * Restore the old SIGUSR1 signal handler and mask
	 */
	sigaction(SIGUSR1, &osa, NULL);
	sigprocmask(SIG_SETMASK, &osigs, NULL);

	/*
	 * Set creation information.
	 */
	ctx_creating = ctx;
	ctx_creating_func = func;
	memcpy(&ctx_creating_sigs, &osigs, sizeof(sigset_t));

	/*
	 * Now enter the trampoline again, but this time not as a signal
	 * handler. Instead we jump into it directly.
	 */
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
	stack = (char *) stack + CO_STK_COROSIZE;
	co->alloc = alloc;
	co->func = func;
	co->data = data;
	if (co_set_context(&co->ctx, co_runner, stack, size - CO_STK_COROSIZE) < 0) {
		if (alloc)
			free(co);
		return NULL;
	}

	return (coroutine_t) co;
}

void co_delete(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *) coro;

	if (co == tctx->co_curr) {
		fprintf(stderr, "[PCL] Cannot delete itself: curr=%p\n",
			tctx->co_curr);
		exit(1);
	}
	if (co->alloc)
		free(co);
}

void co_call(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *) coro, *oldco = tctx->co_curr;

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
		co_call((coroutine_t) cdh);
		if (tctx->co_dhelper == NULL) {
			fprintf(stderr,
				"[PCL] Resume to delete helper coroutine: curr=%p caller=%p\n",
				tctx->co_curr, tctx->co_curr->caller);
			exit(1);
		}
	}
}

void co_exit_to(coroutine_t coro)
{
	cothread_ctx *tctx = co_get_thread_ctx();
	coroutine *co = (coroutine *) coro;

	if (tctx->dchelper == NULL &&
	    (tctx->dchelper = co_create(co_del_helper, NULL,
					tctx->stk, sizeof(tctx->stk))) == NULL) {
		fprintf(stderr, "[PCL] Unable to create delete helper coroutine: curr=%p\n",
			tctx->co_curr);
		exit(1);
	}
	tctx->co_dhelper = co;

	co_call((coroutine_t) tctx->dchelper);

	fprintf(stderr, "[PCL] Stale coroutine called: curr=%p  exitto=%p  caller=%p\n",
		tctx->co_curr, co, tctx->co_curr->caller);
	exit(1);
}

void co_exit(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();

	co_exit_to((coroutine_t) tctx->co_curr->restarget);
}

coroutine_t co_current(void)
{
	cothread_ctx *tctx = co_get_thread_ctx();

	return (coroutine_t) tctx->co_curr;
}

void *co_get_data(coroutine_t coro)
{
	coroutine *co = (coroutine *) coro;

	return co->data;
}

void *co_set_data(coroutine_t coro, void *data)
{
	coroutine *co = (coroutine *) coro;
	void *odata;

	odata = co->data;
	co->data = data;

	return odata;
}
