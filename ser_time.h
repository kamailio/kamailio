/* 
 * time related functions
 *
 * Copyright (C) 2006 iptelorg GmbH
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

/*!
 * \file
 * \brief Kamailio core :: Time related functions
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */
#ifndef _ser_time_h
#define _ser_time_h

#include <sys/time.h>
#include <time.h>

/* time(2) equivalent, using ser internal timers (faster then a syscall) */
time_t ser_time(time_t* t);

/* gettimeofday(2) equivalent, faster but much more imprecise
 * (in normal conditions should be within 0.1 s of the real time)
 * WARNING: ignores tz (it's obsolete anyway) */
int ser_gettimeofday(struct timeval* tv, const struct timezone *tz);

#endif /* _ser_time_h */
