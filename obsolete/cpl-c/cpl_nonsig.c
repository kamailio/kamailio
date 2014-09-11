/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-06-27: file created (bogdan)
 */

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/uio.h>
#include <signal.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../cfg/cfg_struct.h"
#include "cpl_nonsig.h"
#include "CPL_tree.h"


#define MAX_LOG_FILE_NAME      32
#define MAX_FD                 32

#define FILE_NAME_SUFIX        ".log"
#define FILE_NAME_SUFIX_LEN    (sizeof(FILE_NAME_SUFIX)-1)

#define LOG_SEPARATOR          ": "
#define LOG_SEPARATOR_LEN      (sizeof(LOG_SEPARATOR)-1)

#define DEFAULT_LOG_NAME       "default_log"
#define DEFAULT_LOG_NAME_LEN   (sizeof(DEFAULT_LOG_NAME)-1)

#define LOG_TERMINATOR          "\n"
#define LOG_TERMINATOR_LEN      (sizeof(LOG_TERMINATOR)-1)


static char file[MAX_LOG_DIR_SIZE+1+MAX_LOG_FILE_NAME+FILE_NAME_SUFIX_LEN+1];
static char *file_ptr;


static inline void write_log( struct cpl_cmd *cmd)
{
	struct iovec  wr_vec[5];
	time_t now;
	char *time_ptr;
	int fd;
	int ret;

	/* build file name (cmd->s1 is the user name)*/
	if (cmd->s1.len>MAX_LOG_FILE_NAME)
		cmd->s1.len = MAX_LOG_FILE_NAME;
	memcpy(file_ptr, cmd->s1.s, cmd->s1.len );
	memcpy(file_ptr+cmd->s1.len,FILE_NAME_SUFIX,FILE_NAME_SUFIX_LEN);
	file_ptr[cmd->s1.len+FILE_NAME_SUFIX_LEN] = 0;

	/* get current date+time -> wr_vec[0] */
	time( &now );
	time_ptr = ctime( &now );
	wr_vec[0].iov_base = time_ptr;
	wr_vec[0].iov_len = strlen( time_ptr );
	/* ctime_r adds a \n at the end -> overwrite it with space */
	time_ptr[ wr_vec[0].iov_len-1 ] = ' ';

	/* log name (cmd->s2) ->  wr_vec[1] */
	if (cmd->s2.s==0 || cmd->s2.len==0) {
		wr_vec[1].iov_base = DEFAULT_LOG_NAME;
		wr_vec[1].iov_len = DEFAULT_LOG_NAME_LEN;
	} else {
		wr_vec[1].iov_base = cmd->s2.s;
		wr_vec[1].iov_len = cmd->s2.len;
	}

	/* log separator ->  wr_vec[2] */
	wr_vec[2].iov_base = LOG_SEPARATOR;
	wr_vec[2].iov_len = LOG_SEPARATOR_LEN;

	/* log comment (cmd->s3) ->  wr_vec[3] */
	wr_vec[3].iov_base = cmd->s3.s;
	wr_vec[3].iov_len = cmd->s3.len;

	/* log terminator ->  wr_vec[2] */
	wr_vec[4].iov_base = LOG_TERMINATOR;
	wr_vec[4].iov_len = LOG_TERMINATOR_LEN;

	/* [create+]open the file */
	fd = open( file, O_CREAT|O_APPEND|O_WRONLY, 0664);
	if (fd==-1) {
		LOG(L_ERR,"ERROR:cpl_c:write_log: cannot open file [%s] : %s\n",
			file, strerror(errno) );
		return;
	}
	/* get the log */
	DBG("DEBUG:cpl_c:write_log: logging into [%s]... \n",file);
	/* I'm really not interested in the return code for write ;-) */
	while ( (ret=writev( fd, wr_vec, 5))==-1 ) {
		if (errno==EINTR)
			continue;
		LOG(L_ERR,"ERROR:cpl_c:write_log: writing to log file [%s] : %s\n",
			file, strerror(errno) );
	}
	close (fd);

	shm_free( cmd->s1.s );
}



static inline void send_mail( struct cpl_cmd *cmd)
{
	char *argv[5];
	int pfd[2];
	pid_t  pid;
	int i;

	if (pipe(pfd) < 0) {
		LOG(L_ERR,"ERROR:cpl_c:send_mail: pipe failed: %s\n",strerror(errno));
		return;
	}

	/* even if I haven't fork yet, I push the date on the pipe just to get
	 * rid of one more malloc + copy */
	if (cmd->s3.len && cmd->s3.s) {
		if ( (i=write( pfd[1], cmd->s3.s, cmd->s3.len ))!=cmd->s3.len ) {
			LOG(L_ERR,"ERROR:cpl_c:send_mail: write returned error %s\n",
				strerror(errno));
			goto error;
		}
	}

	if ( (pid = fork()) < 0) {
		LOG(L_ERR,"ERROR:cpl_c:send_mail: fork failed: %s\n",strerror(errno));
		goto error;
	} else if (pid==0) {
		/* child -> close all descriptors excepting pfd[0] */
		for (i=3; i < MAX_FD; i++)
			if (i!=pfd[0])
				close(i);
		if (pfd[0] != STDIN_FILENO) {
			dup2(pfd[0], STDIN_FILENO);
			close(pfd[0]);
		}

		/* set the argument vector*/
		argv[0] = "mail";
		argv[1] = "-s";
		if (cmd->s2.s && cmd->s2.len) {
			/* put the subject in this format : <"$subject"\0> */
			if ( (argv[2]=(char*)pkg_malloc(1+cmd->s2.len+1+1))==0) {
				LOG(L_ERR,"ERROR:cpl_c:send_mail: cannot get pkg memory\n");
				goto child_exit;
			}
			argv[2][0] = '\"';
			memcpy(argv[2]+1,cmd->s2.s,cmd->s2.len);
			argv[2][cmd->s2.len+1] = '\"';
			argv[2][cmd->s2.len+2] = 0;
		} else {
			argv[2] = "\"[CPL notification]\"";
		}
		/* put the TO in <$to\0> format*/
		if ( (argv[3]=(char*)pkg_malloc(cmd->s1.len+1))==0) {
			LOG(L_ERR,"ERROR:cpl_c:send_mail: cannot get pkg memory\n");
			goto child_exit;
		}
		memcpy(argv[3],cmd->s1.s,cmd->s1.len);
		argv[3][cmd->s1.len] = 0;
		/* last element in vector mist be a null pointer */
		argv[4] = (char*)0;
		/* just debug */
		for(i=0;i<sizeof(argv)/sizeof(char*);i++)
			DBG(" argv[%d] = %s\n",i,argv[i]);
		/* once I copy localy all the data from shm mem -> free the shm */
		shm_free( cmd->s1.s );

		/* set an alarm -> sending the email shouldn't take more than 10 sec */
		alarm(10);
		/* run the external mailer */
		DBG("DEBUG:cpl_c:send_mail: new forked process created -> "
			"doing execv..\n");
		execv("/usr/bin/mail",argv);
		/* if we got here means execv exit with error :-( */
		LOG(L_ERR,"ERROR:cpl_c:send_mail: execv failed! (%s)\n",
			strerror(errno));
child_exit:
		_exit(127);
	}

	/* parent -> close both ends of pipe */
	close(pfd[0]);
	close(pfd[1]);
	return;
error:
	shm_free( cmd->s1.s );
	close(pfd[0]);
	close(pfd[1]);
	return;
}




void cpl_aux_process( int cmd_out, char *log_dir)
{
	struct cpl_cmd cmd;
	int len;

	/* this process will ignore SIGCHLD signal */
	if (signal( SIGCHLD, SIG_IGN)==SIG_ERR) {
		LOG(L_ERR,"ERROR:cpl_c:cpl_aux_process: cannot set to IGNORE "
			"SIGCHLD signal\n");
	}

	/* set the path for logging */
	if (log_dir) {
		strcpy( file, log_dir);
		file_ptr = file + strlen(log_dir);
		*(file_ptr++) = '/';
	}

	while(1) {
		/* let's read a command from pipe */
		len = read( cmd_out, &cmd, sizeof(struct cpl_cmd));
		if (len!=sizeof(struct cpl_cmd)) {
			if (len>=0) {
				LOG(L_ERR,"ERROR:cpl_aux_processes: truncated message"
					" read from pipe! -> discarded\n");
			} else if (errno!=EAGAIN) {
				LOG(L_ERR,"ERROR:cpl_aux_process: pipe reading failed: "
					" : %s\n",strerror(errno));
			}
			sleep(1);
			continue;
		}

		/* update the local config */
		cfg_update();

		/* process the command*/
		switch (cmd.code) {
			case CPL_LOG_CMD:
				write_log( &cmd );
				break;
			case CPL_MAIL_CMD:
				send_mail( &cmd );
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_aux_process: unknown command (%d) "
					"received! -> ignoring\n",cmd.code);
		} /* end switch*/

	}
}

