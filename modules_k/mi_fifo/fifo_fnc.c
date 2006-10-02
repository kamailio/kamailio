/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../mi/mi.h"
#include "../../mem/mem.h"
#include "mi_fifo.h"
#include "fifo_fnc.h"
#include "mi_parser.h"
#include "mi_writer.h"

static int  mi_fifo_read = 0;
static int  mi_fifo_write = 0;
static char *mi_buf = 0;
static char *reply_fifo_s = 0;
static int  reply_fifo_len = 0;


FILE* mi_init_fifo_server(char *fifo_name, int mi_fifo_mode,
						int mi_fifo_uid, int mi_fifo_gid, char* fifo_reply_dir)
{
	FILE *fifo_stream;
	long opt;

	/* create FIFO ... */
	if ((mkfifo(fifo_name, mi_fifo_mode)<0)) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: can't create FIFO: "
			"%s (mode=%d)\n",
			strerror(errno), mi_fifo_mode);
		return 0;
	}

	DBG("DEBUG:mi_fifo:mi_init_fifo_server: FIFO created @ %s\n", fifo_name );

	if ((chmod(fifo_name, mi_fifo_mode)<0)) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: can't chmod FIFO: "
			"%s (mode=%d)\n",
			strerror(errno), mi_fifo_mode);
		return 0;
	}

	if ((mi_fifo_uid!=-1) || (mi_fifo_gid!=-1)){
		if (chown(fifo_name, mi_fifo_uid, mi_fifo_gid)<0){
			LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: failed to change "
				"the owner/group for %s  to %d.%d; %s[%d]\n",
				fifo_name, mi_fifo_uid, mi_fifo_gid, strerror(errno), errno);
			return 0;
		}
	}

	DBG("DEBUG:mi_fifo:mi_init_fifo_server: fifo %s opened, mode=%o\n",
		fifo_name, mi_fifo_mode );

	/* open it non-blocking or else wait here until someone
	 * opens it for writing */
	mi_fifo_read=open(fifo_name, O_RDONLY|O_NONBLOCK, 0);
	if (mi_fifo_read<0) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: mi_fifo_read did "
			"not open: %s\n", strerror(errno));
		return 0;
	}

	fifo_stream = fdopen(mi_fifo_read, "r");
	if (fifo_stream==NULL) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: fdopen failed: %s\n",
			strerror(errno));
		return 0;
	}

	/* make sure the read fifo will not close */
	mi_fifo_write=open( fifo_name, O_WRONLY|O_NONBLOCK, 0);
	if (mi_fifo_write<0) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: fifo_write did not "
			"open: %s\n", strerror(errno));
		return 0;
	}
	/* set read fifo blocking mode */
	if ((opt=fcntl(mi_fifo_read, F_GETFL))==-1){
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: fcntl(F_GETFL) "
			"failed: %s [%d]\n", strerror(errno), errno);
		return 0;
	}
	if (fcntl(mi_fifo_read, F_SETFL, opt & (~O_NONBLOCK))==-1){
		LOG(L_ERR, "ERROR:mi_fifo:mi_init_fifo_server: fcntl(F_SETFL) "
			"failed: %s [%d]\n", strerror(errno), errno);
		return 0;
	}

	/* allocate all static buffers */
	mi_buf = pkg_malloc(MAX_MI_FIFO_BUFFER);
	reply_fifo_s = pkg_malloc(MAX_MI_FILENAME);
	if ( mi_buf==NULL|| reply_fifo_s==NULL) {
		LOG(L_ERR,"ERROR:mi_fifo:mi_init_fifo_server: no more pkg memory\n");
		return 0;
	}

	/* init fifo reply dir buffer */
	reply_fifo_len = strlen(fifo_reply_dir);
	memcpy( reply_fifo_s, fifo_reply_dir, reply_fifo_len);

	return fifo_stream;
}



/* reply fifo security checks:
 * checks if fd is a fifo, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * returns 0 if ok, <0 if not */
static int mi_fifo_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;
	
	if (fstat(fd, &fst)<0){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: fstat failed: %s\n",
				strerror(errno));
		return -1;
	}
	/* check if fifo */
	if (!S_ISFIFO(fst.st_mode)){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: %s is not a fifo\n", fname);
		return -1;
	}
	/* check if hard-linked */
	if (fst.st_nlink>1){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: security: fifo_check: "
			"%s is hard-linked %d times\n", fname, (unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if (lstat(fname, &lst)<0){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: lstat failed: %s\n",
				strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: security: fifo_check: %s is "
			"a soft link\n", fname);
		return -1;
	}
	/* if this is not a symbolic link, check to see if the inode didn't
	 * change to avoid possible sym.link, rm sym.link & replace w/ fifo race
	 */
	if ((lst.st_dev!=fst.st_dev)||(lst.st_ino!=fst.st_ino)){
		LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_check: security: fifo_check: "
			"inode/dev number differ: %d %d (%s)\n",
			(int)fst.st_ino, (int)lst.st_ino, fname);
		return -1;
	}
	/* success */
	return 0;
}



static FILE *mi_open_reply_pipe( char *pipe_name )
{
	int fifofd;
	FILE *file_handle;
	int flags;

	int retries=FIFO_REPLY_RETRIES;

	if (!pipe_name || *pipe_name==0) {
		DBG("DEBUG:mi_fifo:mi_open_reply_pipe: no file to write to "
			"about missing cmd\n");
		return 0;
	}

tryagain:
	/* open non-blocking to make sure that a broken client will not 
	 * block the FIFO server forever */
	fifofd=open( pipe_name, O_WRONLY | O_NONBLOCK );
	if (fifofd==-1) {
		/* retry several times if client is not yet ready for getting
		   feedback via a reply pipe
		*/
		if (errno==ENXIO) {
			/* give up on the client - we can't afford server blocking */
			if (retries==0) {
				LOG(L_ERR, "ERROR:mi_fifo:mi_open_reply_pipe: no client "
					"at %s\n",pipe_name );
				return 0;
			}
			/* don't be noisy on the very first try */
			if (retries!=FIFO_REPLY_RETRIES)
				DBG("DEBUG:mi_fifo:mi_open_reply_pipe: retry countdown: %d\n",
					retries );
			sleep_us( FIFO_REPLY_WAIT );
			retries--;
			goto tryagain;
		}
		/* some other opening error */
		LOG(L_ERR, "ERROR:mi_fifo:mi_open_reply_pipe: open error (%s): %s\n",
			pipe_name, strerror(errno));
		return 0;
	}
	/* security checks: is this really a fifo?, is 
	 * it hardlinked? is it a soft link? */
	if (mi_fifo_check(fifofd, pipe_name)<0) goto error;

	/* we want server blocking for big writes */
	if ( (flags=fcntl(fifofd, F_GETFL, 0))<0) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_open_reply_pipe (%s): getfl failed: %s\n",
			pipe_name, strerror(errno));
		goto error;
	}
	flags&=~O_NONBLOCK;
	if (fcntl(fifofd, F_SETFL, flags)<0) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_open_reply_pipe (%s): setfl cntl "
			"failed: %s\n", pipe_name, strerror(errno));
		goto error;
	}

	/* create an I/O stream */
	file_handle=fdopen( fifofd, "w");
	if (file_handle==NULL) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_open_reply_pipe: open error (%s): %s\n",
			pipe_name, strerror(errno));
		goto error;
	}
	return file_handle;
error:
	close(fifofd);
	return 0;
}



int mi_read_line( char *b, int max, FILE *stream, int *read)
{
	int retry_cnt;
	int len;
	retry_cnt=0;

retry:
	if (fgets(b, max, stream)==NULL) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_read_line: fifo_server fgets failed: "
			"%s\n", strerror(errno));
		/* on Linux, fgets sometimes returns ESPIPE -- give
		   it few more chances
		*/
		if (errno==ESPIPE) {
			retry_cnt++;
			if (retry_cnt<4) goto retry;
		}
		/* interrupted by signal or ... */
		if ((errno==EINTR)||(errno==EAGAIN)) goto retry;
		kill(0, SIGTERM);
	}
	/* if we did not read whole line, our buffer is too small
	   and we cannot process the request; consume the remainder of 
	   request
	*/

	len=strlen(b);
	if (len && !(b[len-1]=='\n' || b[len-1]=='\r')) {
		LOG(L_ERR, "ERROR:mi_fifo:mi_read_line: request  line too long\n");
		return -1;
	}
	*read = len;

	return 0;
}



static char *get_reply_filename( char * file, int len )
{
	if ( strchr(file,'.') || strchr(file,'/') || strchr(file, '\\') ) {
		LOG(L_ERR, "ERROR:mi_fifo:get_reply_filename: forbidden filename: "
			"%s\n", file);
		return 0;
	}

	if (reply_fifo_len + len + 1 > MAX_MI_FILENAME) {
		LOG(L_ERR, "ERROR:mi_fifo:get_reply_filename: reply fifoname "
			"too long %d\n",reply_fifo_len + len);
		return 0;
	}

	memcpy( reply_fifo_s+reply_fifo_len, file, len );
	reply_fifo_s[reply_fifo_len+len]=0;

	return reply_fifo_s;
}



void mi_fifo_server(FILE *fifo_stream)
{
	struct mi_node *mi_cmd;
	struct mi_node *mi_rpl;
	int line_len;
	char *file_sep, *command, *file;
	struct mi_cmd *f;
	FILE *reply_stream;

	while(1) {
		reply_stream = NULL;

		/* commands must look this way ':<command>:[filename]' */
		if (mi_read_line(mi_buf,MAX_MI_FIFO_BUFFER,fifo_stream, &line_len)) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: failed to read "
				"command\n");
			goto consume;
		}

		/* trim from right */
		while(line_len) {
			if(mi_buf[line_len-1]=='\n' || mi_buf[line_len-1]=='\r'
				|| mi_buf[line_len-1]==' ' || mi_buf[line_len-1]=='\t' ) {
				line_len--;
				mi_buf[line_len]=0;
			} else break;
		} 

		if (line_len==0) {
			LOG(L_DBG, "INFO:mi_fifo:mi_fifo_server: command empty\n");
			continue;
		}
		if (line_len<3) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: command must have "
				"at least 3 chars\n");
			goto consume;
		}
		if (*mi_buf!=MI_CMD_SEPARATOR) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: command must begin "
				"with %c: %.*s\n", MI_CMD_SEPARATOR, line_len, mi_buf );
			goto consume;
		}
		command = mi_buf+1;
		file_sep=strchr(command, MI_CMD_SEPARATOR );
		if (file_sep==NULL) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: file separator "
				"missing\n");
			goto consume;
		}
		if (file_sep==command) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: empty command\n");
			goto consume;
		}
		if (*(file_sep+1)==0) {
			file = NULL;
		} else {
			file = file_sep+1;
			file = get_reply_filename(file, mi_buf+line_len-file);
			if (file==NULL) {
				LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: trimming "
					"filename\n");
				goto consume;
			}
		}
		/* make command zero-terminated */
		*file_sep=0;

		reply_stream = mi_open_reply_pipe( file );
		if (reply_stream==NULL) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: cannot open reply pipe "
				"%s\n", file);
			goto consume;
		}

		f=lookup_mi_cmd( command, strlen(command) );
		if (f==0) {
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: command %s is not "
				"available\n", command);
			mi_fifo_reply( reply_stream, "500 command '%s' not available\n",
				command);
			goto consume;
		}

		mi_cmd = mi_parse_tree(fifo_stream);
		if (mi_cmd==NULL){
			mi_fifo_reply( reply_stream, "400 parse error in command '%s'\n",
				command);
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server:error parsing mi tree\n");
			continue;
		}

		DBG("DEBUG:mi_fifo:mi_fifo_server: done parsing the mi tree\n");

		if ( (mi_rpl=run_mi_cmd(f, mi_cmd))==0 ){
			mi_fifo_reply(reply_stream, "500 command '%s' failed\n", command);
			LOG(L_ERR, "ERROR:mi_fifo:mi_fifo_server: command (%s) "
				"processing failed\n", command );
		} else {
			mi_write_tree( reply_stream, mi_rpl);
			free_mi_tree( mi_rpl );
		}

		/* close reply fifo */
		fclose(reply_stream);
		/* destroy request tree */
		free_mi_tree( mi_cmd );

		continue;

consume:
		DBG("DEBUG:mi_fifo:mi_fifo_server: entered consume\n");
		if (reply_stream)
			fclose(reply_stream);
		/* consume the rest of the fifo request */
		do {
			mi_read_line(mi_buf,MAX_MI_FIFO_BUFFER,fifo_stream,&line_len);
		} while(line_len>1);
		DBG("DEBUG:mi_fifo:mi_fifo_server: **** done consume\n");
	}
}

