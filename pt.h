/*
 * Process Table
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
/** Kamailio core :: internal fork functions and process table.
 * @file: pt.h
 * @ingroup core
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

int init_pt(int proc_no);
int get_max_procs(void);
int register_procs(int no);
int get_max_open_fds(void);
int register_fds(int no);


int close_extra_socks(int proc_id, int proc_no);

#define get_proc_no() ((process_count)?*process_count:0)

/* return processes pid */
int my_pid(void);

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

unsigned int set_fork_delay(unsigned int v);

#endif
