/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(PCL_H)
#define PCL_H

#ifdef __cplusplus
#define PCLXC extern "C"
#else
#define PCLXC
#endif

typedef void *coroutine_t;

PCLXC int co_thread_init(void);
PCLXC void co_thread_cleanup(void);

PCLXC coroutine_t co_create(void (*func)(void *), void *data, void *stack,
			    int size);
PCLXC void co_delete(coroutine_t coro);
PCLXC void co_call(coroutine_t coro);
PCLXC void co_resume(void);
PCLXC void co_exit_to(coroutine_t coro);
PCLXC void co_exit(void);
PCLXC coroutine_t co_current(void);
PCLXC void *co_get_data(coroutine_t coro);
PCLXC void *co_set_data(coroutine_t coro, void *data);

#endif
