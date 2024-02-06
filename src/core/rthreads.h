/*
 * Copyright (C) 2024 Chan Shih-Ping
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * A set of helpers to run functions in threads.
 *
 * This is not a thread pool implementation -
 * - it runs functions in a run-once thread to avoid
 * creating thread-locals in the calling thread.
 *
 * Primary use case: to init libssl in a separate thread
 */
#include <pthread.h>

#include "./globals.h"
/*
 * prototype: void *fn(void *arg) { ... }
 */
typedef void *(*_thread_proto)(void *);

#ifndef KSR_RTHREAD_SKIP_P
static void *run_threadP(_thread_proto fn, void *arg)
{
#ifdef USE_TLS
	pthread_t tid;
	void *ret;

        if(likely(ksr_tls_threads_mode == 0 || (ksr_tls_threads_mode == 1 && process_no > 0))) {
            return fn(arg);
        }

	pthread_create(&tid, NULL, fn, arg);
	pthread_join(tid, &ret);

	return ret;
#else
        return fn(arg);
#endif /* USE_TLS */
}
#endif

/*
 * prototype: void *fn(void *arg1, int arg2) { ... }
 */
#ifdef KSR_RTHREAD_NEED_PI
typedef void *(*_thread_protoPI)(void *, int);
struct _thread_argsPI
{
	_thread_protoPI fn;
	void *tptr;
	int tint;
};
static void *run_thread_wrapPI(struct _thread_argsPI *args)
{
	return (*args->fn)(args->tptr, args->tint);
}

static void *run_threadPI(_thread_protoPI fn, void *arg1, int arg2)
{
#ifdef USE_TLS
	pthread_t tid;
	void *ret;

        if(likely(ksr_tls_threads_mode == 0 || (ksr_tls_threads_mode == 1 && process_no > 0))) {
            return fn(arg1, arg2);
        }

	pthread_create(&tid, NULL, (_thread_proto)&run_thread_wrapPI,
			&(struct _thread_argsPI){fn, arg1, arg2});
	pthread_join(tid, &ret);

	return ret;
#else
        return fn(arg1, arg2);
#endif
}
#endif

/*
 * prototype: void fn(void) { ... }
 */
#ifdef KSR_RTHREAD_NEED_V
typedef void (*_thread_protoV)(void);
struct _thread_argsV
{
	_thread_protoV fn;
};
static void *run_thread_wrapV(struct _thread_argsV *args)
{
	(*args->fn)();
	return NULL;
}

static void run_threadV(_thread_protoV fn)
{
#ifdef USE_TLS
	pthread_t tid;

        if(likely(ksr_tls_threads_mode == 0 || (ksr_tls_threads_mode == 1 && process_no > 0))) {
            fn();
        }

	pthread_create(&tid, NULL, (_thread_proto)run_thread_wrapV,
			&(struct _thread_argsV){fn});
	pthread_join(tid, NULL);
#else
        fn();
#endif
}
#endif

/*
 * prototype: int fn(void *, void *) { ... }
 */
#ifdef KSR_RTHREAD_NEED_4PP
typedef int (*_thread_proto4PP)(void *, void *);
struct _thread_args4PP
{
	_thread_proto4PP fn;
	void *arg1;
	void *arg2;
	int *ret;
};
static void *run_thread_wrap4PP(struct _thread_args4PP *args)
{
	*args->ret = (*args->fn)(args->arg1, args->arg2);
	return NULL;
}

static int run_thread4PP(_thread_proto4PP fn, void *arg1, void *arg2)
{
#ifdef USE_TLS
	pthread_t tid;
	int ret;

        if(likely(ksr_tls_threads_mode == 0 || (ksr_tls_threads_mode == 1 && process_no > 0))) {
            return fn(arg1, arg2);
        }
	pthread_create(&tid, NULL, (_thread_proto)run_thread_wrap4PP,
			&(struct _thread_args4PP){fn, arg1, arg2, &ret});
	pthread_join(tid, NULL);

	return ret;
#else
        return fn(arg1, arg2);
#endif
}
#endif

/*
 * prototype: void fn(void *) { ... }
 */
#ifdef KSR_RTHREAD_NEED_0P
typedef void (*_thread_proto0P)(void *);
struct _thread_args0P
{
	_thread_proto0P fn;
	void *arg1;
};
static void *run_thread_wrap0P(struct _thread_args0P *args)
{
	(*args->fn)(args->arg1);
	return NULL;
}

static void run_thread0P(_thread_proto0P fn, void *arg1)
{
#ifdef USE_TLS
	pthread_t tid;

        if(likely(ksr_tls_threads_mode == 0 || (ksr_tls_threads_mode == 1 && process_no > 0))) {
            fn(arg1);
        }
	pthread_create(&tid, NULL, (_thread_proto)run_thread_wrap0P,
			&(struct _thread_args0P){fn, arg1});
	pthread_join(tid, NULL);
#else
        fn(arg1)
#endif
}
#endif
