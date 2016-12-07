/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Exec module:: Kill
 * \ingroup exec 
 * Module: \ref exec
 *
 * in this file, we implement the ability to send a kill signal to
 * a child after some time; its a quick ugly hack, for example kill
 * is sent without any knowledge whether the kid is still alive
 *
 * also, it was never compiled without FAST_LOCK -- nothing will
 * work if you turn it off
 *
 * there is also an ugly s/HACK
 *
 * and last but not least -- we don't know the child pid (we use popen)
 * so we cannot close anyway
 *
 * From the README:
 *  (There is kill.c but it is not used along with the current mechanisms based on popen. Besides that kill.c is ugly).
 */


#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include "../../mem/shm_mem.h" 
#include "../../dprint.h"
#include "../../timer.h"
#include "../../locking.h"

#include "kill.h"


static gen_lock_t *kill_lock=NULL;


static struct timer_list kill_list;



#define lock() lock_get(kill_lock)

#define unlock() lock_release(kill_lock)



/* copy and paste from TM -- might consider putting in better
   in some utils part of core
*/
static void timer_routine(unsigned int ticks , void * attr)
{
	struct timer_link *tl, *tmp_tl, *end, *ret;
	int killr;

	/* check if it worth entering the lock */
	if (kill_list.first_tl.next_tl==&kill_list.last_tl 
			|| kill_list.first_tl.next_tl->time_out > ticks )
		return;

	lock();
	end = &kill_list.last_tl;
	tl = kill_list.first_tl.next_tl;
	while( tl!=end && tl->time_out <= ticks ) {
		tl=tl->next_tl;
	}

	/* nothing to delete found */
	if (tl->prev_tl==&kill_list.first_tl) {
		unlock();
		return;
	}
	/* the detached list begins with current beginning */
	ret = kill_list.first_tl.next_tl;
	/* and we mark the end of the split list */
	tl->prev_tl->next_tl = 0;
	/* the shortened list starts from where we suspended */
	kill_list.first_tl.next_tl = tl;
	tl->prev_tl = & kill_list.first_tl;
	unlock();

	/* process the list now */
	while (ret) {
		tmp_tl=ret->next_tl;
		ret->next_tl=ret->prev_tl=0;
		if (ret->time_out>0) {
			killr=kill(ret->pid, SIGTERM );
			LM_DBG("child process (%d) kill status: %d\n", ret->pid, killr );
		}
		shm_free(ret);
		ret=tmp_tl;
	}
}

int schedule_to_kill( int pid )
{
	struct timer_link *tl;
	tl=shm_malloc( sizeof(struct timer_link) );
	if (tl==0) {
		LM_ERR("no shmem\n");
		return -1;
	}
	memset(tl, 0, sizeof(struct timer_link) );
	lock();
	tl->pid=pid;
	tl->time_out=get_ticks()+time_to_kill;
	tl->prev_tl=kill_list.last_tl.prev_tl;
	tl->next_tl=&kill_list.last_tl;
	kill_list.last_tl.prev_tl=tl;
	tl->prev_tl->next_tl=tl;
	unlock();
	return 1;
}

int initialize_kill(void)
{
	/* if disabled ... */
	if (time_to_kill==0) return 1;
    if ((register_timer( timer_routine,
            0 /* param */, 1 /* period */)<0)) {
        LM_ERR("no exec timer registered\n");
        return -1;
    }
	kill_list.first_tl.next_tl=&kill_list.last_tl;
	kill_list.last_tl.prev_tl=&kill_list.first_tl;
	kill_list.first_tl.prev_tl=
	kill_list.last_tl.next_tl = 0;
	kill_list.last_tl.time_out=-1;
	kill_lock=lock_alloc();
	if (kill_lock==0) {
		LM_ERR("no shm mem for mutex\n");
		return -1;
	}
	lock_init(kill_lock);
	LM_DBG("kill initialized\n");
	return 1;
}

void destroy_kill(void)
{
	/* if disabled ... */
	if (time_to_kill==0) 
		return; 
	if (kill_lock) {
		lock_destroy(kill_lock);
		lock_dealloc(kill_lock);
	}
	return;
}
