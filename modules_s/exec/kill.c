/*
 *
 * $Id$
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
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/sem.h>

#include "../../mem/shm_mem.h" 
#include "../../dprint.h"
#include "../../timer.h"
#ifdef FAST_LOCK
#include "../../fastlock.h"
#endif

#include "kill.h"


#ifndef FAST_LOCK
static int semaphore;
#else
static fl_lock_t *kill_lock;
#endif


static struct timer_list kill_list;


#ifndef FAST_LOCK
int change_semaphore( int val )
{
	struct sembuf pbuf;
	int r;
	
	pbuf.sem_num = 0;
	pbuf.sem_op=val;
	pbuf.sem_flag=0;

again:
	r=semop(semaphore, &pbuf, 1 /* 1 operation */ );
	if (r==-1) {
		if (errno==EINTR) {
			DBG("DEBUG: EINTR signal received in change_semaphore\n");
			goto again;
		} else {
			LOG(L_ERR, "ERROR: change_semaphore: %s\n", strerror(errno));
		}
	return r;
}
#endif
	

#ifdef FAST_LOCK
inline static int lock()
{
	get_lock(kill_lock);
	return 0;
}
inline static int unlock() 
{
	release_lock(kill_lock);
	return 0;
}
#else
inline static int lock( ser_lock_t l )
{
	return change_semaphore(semaphore, -1);
}
inline static int unlock( ser_lock_t l )
{
	return change_semaphore(semaphore, +1);
}

static void release_semaphore()
{
	semctl(semaphore, 0, IPC_RMID, 0);
}

static int init_semaphore()
{

	union semun {
		int val;
		struct semid_ds *buf;
		ushort *array;
	} argument;

	semaphore=semget( IPC_PRIVATE, 1, IPC_CREATE | IPC_PERMISSION );
	if (semaphore==-1) {
		LOG(L_ERR, "ERROR: init_lock: semaphore allocation failed\n");
		return -1;
	}
	
	/* binary lock */
	argument.val=+1;
	if (semctl( semaphore, 0 , SETVAL , argument )==-1) {
		LOG(L_ERR, "ERROR: init_lock: semaphore init failed\n");
		relase_semaphore();
		return -1;
	}
}
#endif


/* copy and paste from TM -- might consider putting in better
   in some utils part of core
*/
static void timer_routine(unsigned int ticks , void * attr)
{
	struct timer_link *tl, *tmp_tl, *end, *ret;
	int killr;

	/* check if it wirth entering the lock */
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
			DBG("DEBUG: child process (%d) kill status: %d\n",
				ret->pid, killr );
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
		LOG(L_ERR, "ERROR: schedule_to_kill: no shmem\n");
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

int initialize_kill()
{
	/* if disabled ... */
	if (time_to_kill==0) return 1;
    if ((register_timer( timer_routine,
            0 /* param */, 1 /* period */)<0)) {
        LOG(L_ERR, "ERROR: kill_initialize: no exec timer registered\n");
        return -1;
    }
	kill_list.first_tl.next_tl=&kill_list.last_tl;
	kill_list.last_tl.prev_tl=&kill_list.first_tl;
	kill_list.first_tl.prev_tl=
		kill_list.last_tl.next_tl = 0;
	kill_list.last_tl.time_out=-1;
#ifdef FAST_LOCK
	kill_lock=shm_malloc(sizeof(fl_lock_t));
	if (kill_lock==0) {
		LOG(L_ERR, "ERROR: initialize_kill: no mem for mutex\n");
		return -1;
	}
	init_lock(*kill_lock);
	DBG("DEBuG: kill initialized\n");
	return 1;
#else
	return init_semaphore();
#endif
}

void destroy_kill()
{
	/* if disabled ... */
	if (time_to_kill==0) 
		return; 
#ifdef FAST_LOCK
	/* HACK -- if I don't have casting here, Wall complaints --
	   I have no clue why 
	*/
	shm_free((void *)kill_lock);
#else
	release_semaphore();
#endif
	return;
}
