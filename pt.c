/*
 * $Id$
 *
 * Process Table
 *
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2006-06-14	added process table in shared mem (dragos)
 */


#include "pt.h"
#include "tcp_init.h"
#include "sr_module.h"

#include "stdio.h"


static int estimated_proc_no=0;

/* returns 0 on success, -1 on error */
int init_pt(int proc_no)
{
	estimated_proc_no+=proc_no;
	/*alloc pids*/
#ifdef SHM_MEM
	pt=shm_malloc(sizeof(struct process_table)*estimated_proc_no);
	process_count = shm_malloc(sizeof(int));
#else
	pt=pkg_malloc(sizeof(struct process_table)*estimated_proc_no);
	process_count = pkg_malloc(sizeof(int));
#endif
	process_lock = lock_alloc();
	process_lock = lock_init(process_lock);
	if (pt==0||process_count==0||process_lock==0){
		LOG(L_ERR, "ERROR: out  of memory\n");
		return -1;
	}
	memset(pt, 0, sizeof(struct process_table)*estimated_proc_no);

	process_no=0; /*main process number*/
	pt[process_no].pid=getpid();
	memcpy(pt[*process_count].desc,"main",5);
	*process_count=1;
	return 0;
}


/* register no processes, used from mod_init when processes will be forked
 *  from mod_child 
 *  returns 0 on success, -1 on error
 */
int register_procs(int no)
{
	if (pt){
		LOG(L_CRIT, "BUG: register_procs(%d) called at runtime\n", no);
		return -1;
	}
	estimated_proc_no+=no;
	return 0;
}



/* returns the maximum number of processes */
int get_max_procs()
{
	return estimated_proc_no;
}


/* return processes pid */
inline int my_pid()
{
	return pt ? pt[process_no].pid : getpid();
}

/**
 * Forks a new process.
 * @param child_id - rank, if equal to PROC_NOCHLDINIT init_child will not be
 *                   called for the new forked process (see sr_module.h)
 * @param desc - text description for the process table
 * @param make_sock - if to create a unix socket pair for it
 * @returns the pid of the new process
 */
inline int fork_process(int child_id, char *desc, int make_sock)
{
	int pid,old_process_no;
#ifdef USE_TCP
	int sockfd[2];
#endif

	lock_get(process_lock);	
	if (*process_count>=estimated_proc_no) {
		LOG(L_CRIT, "ERROR: fork_process(): Process limit of %d exceeded."
					" Will simulate fork fail.\n", estimated_proc_no);
		lock_release(process_lock);
		return -1;
	}	
	
	#ifdef USE_TCP
		if(make_sock && !tcp_disable){
			 if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
				LOG(L_ERR, "ERROR: fork_process(): socketpair failed: %s\n",
					strerror(errno));
				return -1;
			}
		}
	#endif
	
	old_process_no = process_no;
	process_no = *process_count;
	pid = fork();
	if (pid<0) {
		lock_release(process_lock);
		return pid;
	}
	if (pid==0){
		/* child */
		/* wait for parent to get out of critical zone.
		 * this is actually relevant as the parent updates
		 * the pt & process_count. */
		lock_get(process_lock);
		#ifdef USE_TCP
			if (make_sock && !tcp_disable){
				close(sockfd[0]);
				unix_tcp_sock=sockfd[1];
			}
		#endif		
		lock_release(process_lock);	
		if ((child_id!=PROC_NOCHLDINIT) && (init_child(child_id) < 0)) {
			LOG(L_ERR, "ERROR: fork_process(): init_child failed for %s\n",
						pt[process_no].desc);
			return -1;
		}
		return pid;
	} else {
		/* parent */
		process_no = old_process_no;
		/* add the process to the list in shm */
		pt[*process_count].pid=pid;
		if (desc){
			strncpy(pt[*process_count].desc, desc, MAX_PT_DESC);
		}
		#ifdef USE_TCP
			if (make_sock && !tcp_disable){
				close(sockfd[1]);
				pt[*process_count].unix_sock=sockfd[0];
				pt[*process_count].idx=-1; /* this is not "tcp" process*/
			}
		#endif		
		*process_count = (*process_count) +1;
		lock_release(process_lock);
		return pid;
	}
}

/**
 * Forks a new TCP process.
 * @param desc - text description for the process table
 * @param r - index in the tcp_children array
 * @param *reader_fd_1 - pointer to return the reader_fd[1]
 * @returns the pid of the new process
 */
inline int fork_tcp_process(int child_id,char *desc,int r,int *reader_fd_1)
{
	int pid,old_process_no;
	int sockfd[2];
	int reader_fd[2]; /* for comm. with the tcp children read  */


	
	lock_get(process_lock);
	/* set the local process_no */
	if (*process_count>=estimated_proc_no) {
		LOG(L_CRIT, "ERROR: fork_tcp_process(): Process limit of %d exceeded."
					" Simulating fork fail\n", estimated_proc_no);
		return -1;
	}	
	
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
		LOG(L_ERR, "ERROR: fork_tcp_process(): socketpair failed: %s\n",
					strerror(errno));
		return -1;
	}
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, reader_fd)<0){
		LOG(L_ERR, "ERROR: fork_tcp_process(): socketpair failed: %s\n",
					strerror(errno));
		return -1;
	}
	if (tcp_fix_child_sockets(reader_fd)<0){
		LOG(L_ERR, "ERROR: fork_tcp_process(): failed to set non blocking"
					"on child sockets\n");
		/* continue, it's not critical (it will go slower under
		 * very high connection rates) */
	}
	
	old_process_no = process_no;
	process_no = *process_count;
	pid = fork();
	if (pid<0) {
		lock_release(process_lock);
		return pid;
	}
	if (pid==0){
		/* wait for parent to get out of critical zone */
		lock_get(process_lock);
			close(sockfd[0]);
			unix_tcp_sock=sockfd[1];
			if (reader_fd_1) *reader_fd_1=reader_fd[1];
		lock_release(process_lock);
		if (init_child(child_id) < 0) {
			LOG(L_ERR, "ERROR: fork_tcp_process(): init_child failed for "
					"%s\n", pt[process_no].desc);
			return -1;
		}
		return pid;
	} else {
		/* parent */		
		process_no = old_process_no;
		/* add the process to the list in shm */
		pt[*process_count].pid=pid;
		pt[*process_count].unix_sock=sockfd[0];
		pt[*process_count].idx=r; 	
		if (desc){
			snprintf(pt[*process_count].desc, MAX_PT_DESC, "%s child=%d", 
						desc, r);
		}
		
		close(sockfd[1]);
		close(reader_fd[1]);
		
		tcp_children[r].pid=pid;
		tcp_children[r].proc_no=process_no;
		tcp_children[r].busy=0;
		tcp_children[r].n_reqs=0;
		tcp_children[r].unix_sock=reader_fd[0];
		
		*process_count = (*process_count) +1;
		lock_release(process_lock);
		return pid;
	}
}



