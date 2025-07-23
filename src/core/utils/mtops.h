/**
 * Copyright (C) 2025 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _KSR_MTOPS_
#define _KSR_MTOPS_

#include <pthread.h>

/**
 *
 */
typedef struct ksr_sigsem
{
	pthread_mutex_t mtx;
	pthread_cond_t cnd;
	volatile unsigned int val;
} ksr_sigsem_t;

ksr_sigsem_t *ksr_sigsem_alloc(void);

ksr_sigsem_t *ksr_sigsem_xalloc(void);

int ksr_sigsem_init(ksr_sigsem_t *sgs);

void ksr_sigsem_signal(ksr_sigsem_t *sgs);

void ksr_sigsem_wait(ksr_sigsem_t *sgs);

void ksr_sigsem_destroy(ksr_sigsem_t *sgs);

void ksr_sigsem_free(ksr_sigsem_t *sgs);

void ksr_sigsem_xfree(ksr_sigsem_t *sgs);

#endif
