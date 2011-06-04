/* 
 * $Id$
 * 
 * Copyright (C) 2009 iptelorg GmbH
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
 * History:
 * --------
 *  2009-03-10  initial version (andrei)
*/

/**
 * @file
 * @brief SIP-router core :: timer_proc.h - separate process timers
 *
 * (unrelated to the main fast and slow timers)
 * @ingroup core
 * Module: @ref core
 */

#ifndef __timer_proc_h
#define __timer_proc_h

#include "local_timer.h"

/** @brief register the number of extra dummy timer processes */
int register_dummy_timers(int timers);

/** @brief forks a separate simple sleep() periodic timer */
int fork_dummy_timer(int child_id, char* desc, int make_sock,
						timer_function* f, void* param, int interval);

/** @briefforks a timer process based on the local timer */
int fork_local_timer_process(int child_id, char* desc, int make_sock,
						struct local_timer** lt_h);

#endif /*__timer_proc_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
