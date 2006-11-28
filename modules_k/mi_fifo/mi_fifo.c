/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2006-09-25  first version (bogdan)
 */



#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../mi/mi.h"
#include "mi_fifo.h"
#include "mi_parser.h"
#include "mi_writer.h"
#include "fifo_fnc.h"

static int mi_mod_init(void);
static int mi_child_init(int rank);
static int mi_destroy(void);

/* FIFO server vars */
/* FIFO name */
static char *mi_fifo = 0;
/* dir where reply fifos are allowed */
static char *mi_fifo_reply_dir = DEFAULT_MI_REPLY_DIR;
static char *mi_reply_indent = DEFAULT_MI_REPLY_IDENT;
pid_t *mi_fifo_pid = 0;
static int  mi_fifo_uid = -1;
static char *mi_fifo_uid_s = 0;
static int  mi_fifo_gid = -1;
static char *mi_fifo_gid_s = 0;
static int  mi_fifo_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP; /* rw-rw---- */
static int  read_buf_size = MAX_MI_FIFO_READ;

MODULE_VERSION

static param_export_t mi_params[] = {
	{"fifo_name",        STR_PARAM, &mi_fifo},
	{"fifo_mode",        INT_PARAM, &mi_fifo_mode},
	{"fifo_group",       STR_PARAM, &mi_fifo_gid_s},
	{"fifo_group",       INT_PARAM, &mi_fifo_gid},
	{"fifo_user",        STR_PARAM, &mi_fifo_uid_s},
	{"fifo_user",        INT_PARAM, &mi_fifo_uid},
	{"reply_dir",        STR_PARAM, &mi_fifo_reply_dir},
	{"reply_indent",      STR_PARAM, &mi_reply_indent},
	{0,0,0}
};



struct module_exports exports = {
	"mi_fifo",                     /* module name */
	DEFAULT_DLFLAGS,               /* dlopen flags */
	0,                             /* exported functions */
	mi_params,                     /* exported parameters */
	0,                             /* exported statistics */
	0,                             /* exported MI functions */
	0,                             /* exported pseudo-variables */
	mi_mod_init,                   /* module initialization function */
	(response_function) 0,         /* response handling function */
	(destroy_function) mi_destroy, /* destroy function */
	mi_child_init                  /* per-child init function */
};



static int mi_mod_init(void)
{
	int n;
	struct stat filestat;

	/* checking the mi_fifo module param */
	if (mi_fifo==NULL || *mi_fifo == 0) {
		LOG(L_ERR, "ERROR:mi_fifo:mod_init:no fifo configured\n");
		return -1;
	}

	DBG("DBG: mi_fifo: mi_mod_init: testing fifo existance ...\n");
	n=stat(mi_fifo, &filestat);
	if (n==0){
		/* FIFO exist, delete it (safer) */
		if (unlink(mi_fifo)<0){
			LOG(L_ERR, "ERROR: mi_fifo: mi_mod_init: cannot delete old "
				"fifo (%s): %s\n", mi_fifo, strerror(errno));
			return -1;
		}
	}else if (n<0 && errno!=ENOENT){
		LOG(L_ERR, "ERROR: mi_fifo: mi_mod_init: FIFO stat failed: %s\n",
			strerror(errno));
		return -1;
	}

	/* checking the mi_fifo_reply_dir param */
	if(!mi_fifo_reply_dir || *mi_fifo_reply_dir == 0){
		LOG(L_ERR, "ERROR:mi_fifo:mod_init: mi_fifo_reply_dir parameter "
			"is empty\n");
		return -1;
	}

	n = stat(mi_fifo_reply_dir, &filestat);
	if(n < 0){
		LOG(L_ERR, "ERROR: mi_fifo: mi_mod_init: directory stat failed: %s\n",
			strerror(errno));
		return -1;
	}

	if(S_ISDIR(filestat.st_mode) == 0){
		LOG(L_ERR, "ERROR:mi_fifo:mi_mod_init: mi_fifo_reply_dir parameter "
			"is not a directory\n");
		return -1;
	}

	/* check mi_fifo_mode */
	if(!mi_fifo_mode){
		LOG(L_WARN, "WARNING:mi_fifo:mi_mod_init: cannot specify "
			"mi_fifo_mode = 0, forcing it to rw-------\n");
		mi_fifo_mode = S_IRUSR| S_IWUSR;
	}

	if (mi_fifo_uid_s){
		if (user2uid(&mi_fifo_uid, &mi_fifo_gid, mi_fifo_uid_s)<0){
			LOG(L_ERR, "ERROR:mi_fifo:mi_mod_init:bad user name %s\n",
				mi_fifo_uid_s);
			return -1;
		}
	}

	if (mi_fifo_gid_s){
		if (group2gid(&mi_fifo_gid, mi_fifo_gid_s)<0){
			LOG(L_ERR, "ERROR:mi_fifo:mi_mod_init:bad group name %s\n",
				mi_fifo_gid_s);
			return -1;
		}
	}

	/* create the shared memory where the mi_fifo_pid is kept */
	mi_fifo_pid = (pid_t *)shm_malloc(sizeof(pid_t));
	if(!mi_fifo_pid){
		LOG(L_ERR, "ERROR:mi_fifo:mi_mod_init:cannot allocate shared "
			"memory for the mi_fifo_pid\n");
		return -1;
	}

	*mi_fifo_pid = 0;

	return 0;
}


static int mi_child_init(int rank)
{
	FILE *fifo_stream;

	if(rank != 1)
		return 0;

	*mi_fifo_pid = fork();

	if (*mi_fifo_pid < 0){
		LOG(L_ERR, "ERROR:mi_fifo:mi_child_init: the process cannot "
			"fork!\n");
		return -1;
	} else if (*mi_fifo_pid) {
		LOG(L_INFO,"INFO:mi_fifo:mi_child_init(%d): extra fifo listener "
			"processes created\n",rank);
		return 0;
	}

	DBG("DEBUG:mi_fifo:mi_child_init: new process with pid = %d "
		"created.\n",getpid());

	fifo_stream = mi_init_fifo_server( mi_fifo, mi_fifo_mode,
		mi_fifo_uid, mi_fifo_gid, mi_fifo_reply_dir);
	if ( fifo_stream==NULL ) {
		LOG(L_CRIT, "CRITICAL:mi_fifo:mi_child_init: The function "
			"mi_init_fifo_server returned with error!!!\n");
		exit(-1);
	}

	if( init_mi_child()!=0) {
		LOG(L_CRIT,"CRITICAL:mi_fifo:mi_child_init: faild to init the "
			"mi process\n");
		exit(-1);
	}

	if ( mi_parser_init(read_buf_size)!=0 ) {
		LOG(L_CRIT, "CRITICAL:mi_fifo:mi_child_init: failed to init "
			"the command parser\n");
		exit(-1);
	}

	if ( mi_writer_init(read_buf_size, mi_reply_indent)!=0 ) {
		LOG(L_CRIT, "CRITICAL:mi_fifo:mi_child_init: failed to init "
			"the reply writer\n");
		exit(-1);
	}

	mi_fifo_server( fifo_stream );

	LOG(L_CRIT, "CRITICAL:mi_fifo:mi_child_init: the "
		"function mi_fifo_server returned with error!!!\n");
	exit(-1);
}


static int mi_destroy(void)
{
	int n;
	struct stat filestat;

	if(!mi_fifo_pid){
		LOG(L_INFO, "INFO:mi_fifo:mi_destroy:memory for the child's "
			"mi_fifo_pid was not allocated -> nothing to destroy\n");
		return 0;
	}

	/* killing the first child */
	if (!*mi_fifo_pid) {
		LOG(L_INFO,"INFO:mi_fifo:mi_destroy: process hasn't been created -> "
			"nothing to kill\n");
	} else {
		if (kill( *mi_fifo_pid, SIGKILL)!=0) {
			if (errno==ESRCH) {
				LOG(L_INFO,"INFO:mi_fifo:mi_destroy: seems that fifo child is "
					"already dead!\n");
			} else {
				LOG(L_ERR,"ERROR:mi_fifo:mi_destroy: killing the aux. process "
					"failed! kill said: %s\n",strerror(errno));
				goto error;
			}
		} else {
			LOG(L_INFO,"INFO:mi_fifo:mi_destroy: fifo child successfully "
				"killed!");
		}
	}

	/* destroying the fifo file */
	n=stat(mi_fifo, &filestat);
	if (n==0){
		/* FIFO exist, delete it (safer) */
		if (unlink(mi_fifo)<0){
			LOG(L_ERR, "ERROR: mi_fifo: mi_destroy: cannot delete the "
				"fifo (%s): %s\n", mi_fifo, strerror(errno));
			goto error;
		}
	} else if (n<0 && errno!=ENOENT) {
		LOG(L_ERR, "ERROR: mi_fifo: mi_destroy: FIFO stat failed: %s\n",
			strerror(errno));
		goto error;
	}

	/* freeing the shm shared memory */
	shm_free(mi_fifo_pid);

	return 0;
error:
	/* freeing the shm shared memory */
	shm_free(mi_fifo_pid);
	return -1;
}

