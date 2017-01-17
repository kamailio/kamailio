/*
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __DIAMETER_PEER_H
#define __DIAMETER_PEER_H

#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "diameter.h"
#include "diameter_ims.h"
#include "diameter_api.h"

#include "worker.h"

/** Element for the local pid list. */
typedef struct _pid_list_t{
	pid_t pid;
	struct _pid_list_t *next,*prev;
} pid_list_t;

/** local pid list */
typedef struct {
	pid_list_t *head,*tail;
} pid_list_head_t;

pid_t *dp_first_pid;		/**< first pid that we started from		*/

pid_list_head_t *pid_list;	/**< list of local processes			*/
gen_lock_t *pid_list_lock;	/**< lock for list of local processes	*/

int diameter_peer_init_str(str config_str);
int diameter_peer_init(char *cfg_filename);

int diameter_peer_start(int blocking);

void diameter_peer_destroy();


/**
 * Add a pid to the local process list.
 * @param pid newly forked pid
 * @returns 1 on success or 0 on error
 */
static inline int dp_add_pid(pid_t pid)
{
	pid_list_t *n;
	lock_get(pid_list_lock);
	n = shm_malloc(sizeof(pid_list_t));
	if (!n){
		LOG_NO_MEM("shm",sizeof(pid_list_t));
		lock_release(pid_list_lock);
		return 0;
	}
	n->pid = pid;
	n->next = 0;
	n->prev = pid_list->tail;
	if (!pid_list->head) pid_list->head = n;
	if (pid_list->tail) pid_list->tail->next = n;
	pid_list->tail = n;
	lock_release(pid_list_lock);
	return 1;
}

/**
 * Returns the last pid in the local process list.
 */
static inline int dp_last_pid()
{
	int pid;
	lock_get(pid_list_lock);
	if (pid_list->tail)	pid = pid_list->tail->pid;
	else pid = -1;
	lock_release(pid_list_lock);
	return pid;
}

/**
 * Delete a pid from the process list
 * @param pid - the pid to remove
 */
static inline void dp_del_pid(pid_t pid)
{
	pid_list_t *i;
	lock_get(pid_list_lock);
	i = pid_list->head;
	if (!i) {
		lock_release(pid_list_lock);
		return;
	}
	while(i && i->pid!=pid) i = i->next;
	if (i){
		if (i->prev) i->prev->next = i->next;
		else pid_list->head = i->next;
		if (i->next) i->next->prev = i->prev;
		else pid_list->tail = i->prev;
		shm_free(i);
	}
	lock_release(pid_list_lock);
}

#endif
