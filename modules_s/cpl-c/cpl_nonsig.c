/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "cpl_nonsig.h"
#include "CPL_tree.h"

#define MAX_TIME_SIZE          25  /* format = "Wed Jun 30 21:49:08 1993\n" */
#define MAX_LOG_FILE_NAME      32
#define FILE_NAME_SUFIX        ".log"
#define FILE_NAME_SUFIX_LEN    (sizeof(FILE_NAME_SUFIX)-1)


static char file[MAX_LOG_DIR_SIZE+1+MAX_LOG_FILE_NAME+FILE_NAME_SUFIX_LEN+1];
static char *file_ptr;


static inline void write_log( char *s, int user_len, int log_len)
{
	char *log;
	int fd;
	int ret;

	/* build file name */
	if (user_len>MAX_LOG_FILE_NAME)
		user_len = MAX_LOG_FILE_NAME;
	memcpy(file_ptr, s, user_len );
	memcpy(file_ptr+user_len,FILE_NAME_SUFIX,FILE_NAME_SUFIX_LEN);
	file_ptr[user_len+FILE_NAME_SUFIX_LEN] = 0;

	/* [create+]open the file */
	fd = open( file, O_CREAT|O_APPEND|O_WRONLY, 0664);
	if (fd==-1) {
		LOG(L_ERR,"ERROR:cpl_c:write_log: cannot open file [%s] : %s\n",
			file, strerror(errno) );
		return;
	}
	/* get the log */
	log = s + user_len;
	DBG("DEBUG:cpl_c:write_log: logging for [%.*s] into [%s] ->\n|%.*s|\n",
		user_len,s,file,log_len,log);
	/* I'm really not interested in the return code for write ;-) */
	while ( (ret=write(fd,log,log_len))==-1 ) {
		if (errno==EINTR)
			continue;
		LOG(L_ERR,"ERROR:cpl_c:write_log: writing to log file [%s] : %s\n",
			file, strerror(errno) );
	}

	shm_free( s );
}




void cpl_aux_process( int cmd_out, char *log_dir)
{
	struct cpl_cmd cmd;
	int len;

	/* set the path for logging */
	strcpy( file, log_dir);
	file_ptr = file + sizeof(log_dir);
	*(file_ptr++) = '/';

	while(1) {
		/* let's read a command from pipe */
		len = read( cmd_out, &cmd, sizeof(struct cpl_cmd));
		if (len!=sizeof(struct cpl_cmd)) {
			if (len>=0) {
				LOG(L_ERR,"ERROR:cpl_aux_proceress: truncated message"
					" read from pipe! -> discarted\n");
			} else if (errno!=EAGAIN) {
				LOG(L_ERR,"ERROR:cpl_aux_proceress: pipe reading failed: "
					" : %s\n",strerror(errno));
			}
			sleep(1);
			continue;
		}

		/* process the command*/
		switch (cmd.code) {
			case CPL_LOG_CMD:
				write_log( cmd.s, cmd.len1, cmd.len2);
				break;
			case CPL_MAIL_CMD:
				break;
			default:
				LOG(L_ERR,"ERROR:cpl_aux_proceress: unknown command (%d) "
					"recived! -> ignoring\n",cmd.code);
		} /* end switch*/

	}
}

