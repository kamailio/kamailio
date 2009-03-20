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
 *  2003-04-15  added tcp_disable support (andrei)
 *  2006-06-14	added process table in shared mem (dragos)
 *  2007-07-04	added register_fds() and get_max_open_fds(() (andrei)
 */


#ifndef _PT_H
#define _PT_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "globals.h"
#include "timer.h"
#include "socket_info.h"
#include "locking.h"

#define MAX_PT_DESC			128

struct process_table {
	int pid;
#ifdef USE_TCP
	int unix_sock; 	/* unix socket on which tcp main listens	*/
	int idx; 		/* tcp child index, -1 for other processes 	*/
#endif
	char desc[MAX_PT_DESC];
};

extern struct process_table *pt;
extern gen_lock_t* process_lock;
extern int *process_count;
extern int process_no;

extern struct tcp_child* tcp_children;

int init_pt();
int get_max_procs();
int register_procs(int no);
int get_max_open_fds();
int register_fds(int no);


int close_extra_socks(int proc_id, int proc_no);

#define get_proc_no() ((process_count)?*process_count:0)

/* return processes pid */
int my_pid();

/**
 * Forks a new process.
 * @param desc - text description for the process table
 * @param make_sock - if to create a unix socket pair for it
 * @returns the pid of the new process
 */
int fork_process(int child_id,char *desc,int make_sock);

/**
 * Forks a new TCP process.
 * @param desc - text description for the process table
 * @param r - index in the tcp_children array
 * @param *reader_fd_1 - pointer to return the reader_fd[1]
 * @returns the pid of the new process
 */
#ifdef USE_TCP
int fork_tcp_process(int child_id,char *desc,int r,int *reader_fd_1);
#endif

#ifdef PKG_MALLOC
void mem_dump_pkg_cb(str *gname, str *name);
#endif

#ifdef SHM_MEM
int mem_dump_shm_fixup(void *handle, str *gname, str *name, void **val);
#endif

#endif
