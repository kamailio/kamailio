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
 * timer_proc.c  - separate process timers
 * (unrelated to the main fast and slow timers)
 */
/*
 * History:
 * --------
 *  2009-03-10  initial version (andrei)
*/

/**
 * @file
 * @brief SIP-router core ::  timer - separate process timers
 *
 *  (unrelated to the main fast and slow timers)
 *
 * @ingroup core
 * Module: @ref core
 */

#include "timer_proc.h"
#include "cfg/cfg_struct.h"
#include "pt.h"
#include "mem/shm_mem.h"

#include <unistd.h>


/** update internal counters for running new dummy timers
 *  @param timers - number of dummy timer processes
 *  @return - 0 on success; -1 on error
 */
int register_dummy_timers(int timers)
{
	if(register_procs(timers)<0)
		return -1;
	cfg_register_child(timers);
	return 0;
}

/** forks a separate simple sleep() periodic timer.
  * Forks a very basic periodic timer process, that just sleep()s for 
  * the specified interval and then calls the timer function.
  * The new "dummy timer" process execution start immediately, the sleep()
  * is called first (so the first call to the timer function will happen
  * <interval> seconds after the call to fork_dummy_timer)
  * @param child_id - @see fork_process()
  * @param desc     - @see fork_process()
  * @param make_sock - @see fork_process()
  * @param f         - timer function/callback
  * @param param     - parameter passed to the timer function
  * @param interval  - interval in seconds.
  * @return - pid of the new process on success, -1 on error
  *           (doesn't return anything in the child process)
  */
int fork_dummy_timer(int child_id, char* desc, int make_sock,
						timer_function* f, void* param, int interval)
{
	int pid;
	
	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) return -1;
	if (pid==0){
		/* child */
		if (cfg_child_init()) return -1;
		for(;;){
			sleep(interval);
			cfg_update();
			f(get_ticks(), param); /* ticks in s for compatibility with old
									  timers */
		}
	}
	/* parent */
	return pid;
}



/** forks a timer process based on the local timer.
 *  Forks a separate timer process running a local_timer.h type of timer
 *  A pointer to the local_timer handle (allocated in shared memory) is
 *  returned in lt_h. It can be used to add/delete more timers at runtime
 *  (via local_timer_add()/local_timer_del() a.s.o).
 *  If timers are added from separate processes, some form of locking must be
 *  used (all the calls to local_timer* must be enclosed by locks if it
 *  cannot be guaranteed that they cannot execute in the same time)
 *  The timer "engine" must be run manually from the child process. For
 *  example a very simple local timer process that just runs a single 
 *  periodic timer can be started in the following way:
 *      struct local_timer* lt_h;
 *
 *      pid=fork_local_timer_process(...., &lt_h);
 *      if (pid==0){
 *          timer_init(&my_timer, my_timer_f, 0, 0);
 *          local_timer_add(&lt_h, &my_timer, S_TO_TICKS(10), get_ticks_raw());
 *          while(1) { sleep(1); local_timer_run(lt, get_ticks_raw()); }
 *      }
 *
 * @param child_id - @see fork_process()
 * @param desc     - @see fork_process()
 * @param make_sock - @see fork_process()
 * @param lt_h      - local_timer handler
 * @return - pid to the parent, 0 to the child, -1 if error.
 */
int fork_local_timer_process(int child_id, char* desc, int make_sock,
						struct local_timer** lt_h)
{
	int pid;
	struct local_timer* lt;
	
	lt=shm_malloc(sizeof(*lt));
	if (lt==0) goto error;
	if (init_local_timer(lt, get_ticks_raw())<0) goto error;
	pid=fork_process(child_id, desc, make_sock);
	if (pid<0) goto error;
	*lt_h=lt;
	return pid;
error:
	if (lt) shm_free(lt);
	return -1;
}


/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
