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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../dprint.h"
#include "../mem/shm.h"

#include "mtops.h"


/**
 *
 */
ksr_sigsem_t *ksr_sigsem_alloc(void)
{
	ksr_sigsem_t *sgs = NULL;

	sgs = (ksr_sigsem_t *)shm_mallocxz(sizeof(ksr_sigsem_t));
	if(sgs == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}

	return sgs;
}

/**
 *
 */
ksr_sigsem_t *ksr_sigsem_xalloc(void)
{
	ksr_sigsem_t *sgs = NULL;

	sgs = (ksr_sigsem_t *)shm_mallocxz(sizeof(ksr_sigsem_t));
	if(sgs == NULL) {
		SHM_MEM_ERROR;
		return NULL;
	}

	if(ksr_sigsem_init(sgs) < 0) {
		shm_free(sgs);
		return NULL;
	}

	return sgs;
}

/**
 *
 */
int ksr_sigsem_init(ksr_sigsem_t *sgs)
{
	pthread_mutexattr_t mutexattr;
	pthread_condattr_t condattr;

	if(pthread_mutexattr_init(&mutexattr) != 0) {
		LM_ERR("pthread_mutexattr_init failed\n");
		return -1;
	}
	if(pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED) != 0) {
		LM_ERR("pthread_mutexattr_setpshared failed\n");
		return -1;
	}
	if(pthread_mutex_init(&sgs->mtx, &mutexattr) != 0) {
		LM_ERR("pthread_mutex_init failed\n");
		return -1;
	}

	if(pthread_condattr_init(&condattr) != 0) {
		LM_ERR("pthread_condattr_init failed\n");
		return -1;
	}
	if(pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED) != 0) {
		LM_ERR("pthread_condattr_setpshared failed\n");
		return -1;
	}
	if(pthread_cond_init(&sgs->cnd, &condattr) != 0) {
		LM_ERR("pthread_cond_init failed\n");
		return -1;
	}

	return 0;
}

/**
 *
 */
void ksr_sigsem_signal(ksr_sigsem_t *sgs)
{
	pthread_mutex_lock(&sgs->mtx);
	sgs->val = 1;
	pthread_mutex_unlock(&sgs->mtx);
	pthread_cond_signal(&sgs->cnd);
}

/**
 *
 */
void ksr_sigsem_wait(ksr_sigsem_t *sgs)
{
	pthread_mutex_lock(&sgs->mtx);
	while(sgs->val == 0) {
		pthread_cond_wait(&sgs->cnd, &sgs->mtx);
	}
	sgs->val = 0;
	pthread_mutex_unlock(&sgs->mtx);
}

/**
 *
 */
void ksr_sigsem_destroy(ksr_sigsem_t *sgs)
{
	pthread_cond_destroy(&sgs->cnd);
	pthread_mutex_destroy(&sgs->mtx);
}


/**
 *
 */
void ksr_sigsem_free(ksr_sigsem_t *sgs)
{
	shm_free(sgs);
}

/**
 *
 */
void ksr_sigsem_sfree(ksr_sigsem_t *sgs)
{
	pthread_cond_destroy(&sgs->cnd);
	pthread_mutex_destroy(&sgs->mtx);
	shm_free(sgs);
}
