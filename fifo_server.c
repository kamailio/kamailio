/*
 * $Id$
 *
 * simple UAC for things such as SUBSCRIBE or SMS gateway;
 * no authentication and other UAC features -- just send
 * a message, retransmit and await a reply; forking is not
 * supported during client generation, in all other places
 * it is -- adding it should be simple
 */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include "dprint.h"
#include "ut.h"
#include "error.h"
#include "config.h"
#include "globals.h"
#include "fifo_server.h"
#include "mem/mem.h"

/* FIFO server vars */
char *fifo=0; /* FIFO name */
int fifo_mode=S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
pid_t fifo_pid;
/* file descriptors */
static int fifo_read=0;
static int fifo_write=0;
static FILE *fifo_stream;

/* list of fifo command */
static struct fifo_command *cmd_list=0;

static struct fifo_command *lookup_fifo_cmd( char *name )
{
	struct fifo_command *c;
	for(c=cmd_list; c; c=c->next) {
		if (strcasecmp(c->name, name)==0) return c;
	}
	return 0;
}

int register_fifo_cmd(fifo_cmd f, char *cmd_name, void *param)
{
	struct fifo_command *new_cmd;

	if (lookup_fifo_cmd(cmd_name)) {
		LOG(L_ERR, "ERROR: register_fifo_cmd: attempt to register synonyms\n");
		return E_BUG;
	}
	new_cmd=malloc(sizeof(struct fifo_command));
	if (new_cmd==0) {
		LOG(L_ERR, "ERROR: register_fifo_cmd: out of mem\n");
		return E_OUT_OF_MEM;
	}
	new_cmd->f=f;
	new_cmd->name=cmd_name;
	new_cmd->param=param;

	new_cmd->next=cmd_list;
	cmd_list=new_cmd;

	return 1;
}


int read_line( char *b, int max, FILE *stream, int *read )
{
	int len;
	if (fgets(b, max, stream)==NULL) {
		LOG(L_ERR, "ERROR: fifo_server fgets failed: %s\n",
			strerror(errno));
		kill(0, SIGTERM);
	}
	/* if we did not read whole line, our buffer is too small
	   and we cannot process the request; consume the remainder of 
	   request
	*/
	len=strlen(b);
	if (len && !(b[len-1]=='\n' || b[len-1]=='\r')) {
		LOG(L_ERR, "ERROR: read_line: request  line too long\n");
		return 0;
	}
	/* trim from right */
	while(len) {
		if(b[len-1]=='\n' || b[len-1]=='\r'
				|| b[len-1]==' ' || b[len-1]=='\t' ) {
			len--;
			b[len]=0;
		} else break;
	}
	*read=len;
	return 1;
}

static void consume_request( FILE *stream )
{
	int len;
	char buffer[MAX_CONSUME_BUFFER];

	while(!read_line(buffer, MAX_CONSUME_BUFFER, stream, &len));

#ifdef _OBSOLETED
	int eol_count;

	eol_count=0;

	/* each request must be terminated by two EoLs */
	while(eol_count!=2) {
		/* read until EoL is encountered */
		while(!read_line(buffer, MAX_CONSUME_BUFFER, stream, &len));
		eol_count=len==0?eol_count+1:1;
	}
#endif
}

int read_eol( FILE *stream )
{
	int len;
	char buffer[MAX_CONSUME_BUFFER];
	if (!read_line(buffer, MAX_CONSUME_BUFFER, stream, &len) || len!=0) {
		LOG(L_ERR, "ERROR: read_eol: EOL expected: %.10s...\n",
			buffer );
		return 0;
	}
	return 1;
}
	
int read_line_set(char *buf, int max_len, FILE *fifo, int *len)
{
	int set_len;
	char *c;
	int line_len;

	c=buf;set_len=0;
	while(1) {
		if (!read_line(c,max_len,fifo,&line_len)) {
			LOG(L_ERR, "ERROR: fifo_server: line expected\n");
			return 0;
		}
		/* end encountered ... return */
		if (line_len==0) {
			*len=set_len;
			return 1;
		}
		max_len-=line_len; c+=line_len; set_len+=line_len;
		if (max_len<CRLF_LEN) {
			LOG(L_ERR, "ERROR: fifo_server: no place for CRLF\n");
			return 0;
		}
		memcpy(c, CRLF, CRLF_LEN);
		max_len-=CRLF_LEN; c+=CRLF_LEN; set_len+=CRLF_LEN;
	}
}

static char *trim_filename( char * file )
{
	int prefix_len, fn_len;
	char *new_fn;

	/* we only allow files in "/tmp" -- any directory
	   changes are not welcome
	*/
	if (strchr(file,'.') || strchr(file,'/')
				|| strchr(file, '\\')) {
		LOG(L_ERR, "ERROR: trim_filename: forbidden filename: %s\n"
			, file);
		return 0;
	}
	prefix_len=strlen(FIFO_DIR); fn_len=strlen(file);
	new_fn=pkg_malloc(prefix_len+fn_len+1);
	if (new_fn==0) {
		LOG(L_ERR, "ERROR: trim_filename: no mem\n");
		return 0;
	}

	memcpy(new_fn, FIFO_DIR, prefix_len);
	memcpy(new_fn+prefix_len, file, fn_len );
	new_fn[prefix_len+fn_len]=0;

	return new_fn;
}

static void fifo_server(FILE *fifo_stream)
{
	char buf[MAX_FIFO_COMMAND];
	int line_len;
	char *file_sep, *command, *file;
	struct fifo_command *f;

	file_sep=command=file=0;

	while(1) {

		/* commands must look this way ':<command>:[filename]' */
		if (!read_line(buf, MAX_FIFO_COMMAND, fifo_stream, &line_len)) {
			/* line breaking must have failed -- consume the rest
			   and proceed to a new request
			*/
			LOG(L_ERR, "ERROR: fifo_server: command expected\n");
			goto consume;
		}
		if (line_len==0) {
			LOG(L_ERR, "ERROR: fifo_server: command empty\n");
			continue;
		}
		if (line_len<3) {
			LOG(L_ERR, "ERROR: fifo_server: command must have at least 3 chars\n");
			goto consume;
		}
		if (*buf!=CMD_SEPARATOR) {
			LOG(L_ERR, "ERROR: fifo_server: command must start with %c\n", 
				CMD_SEPARATOR);
			goto consume;
		}
		command=buf+1;
		file_sep=strchr(command, CMD_SEPARATOR );
		if (file_sep==NULL) {
			LOG(L_ERR, "ERROR: fifo_server: file separator missing\n");
			goto consume;
		}
		if (file_sep==command) {
			LOG(L_ERR, "ERROR: fifo_server: empty command\n");
			goto consume;
		}
		if (*(file_sep+1)==0) file=NULL; 
		else {
			file=file_sep+1;
			file=trim_filename(file);
			if (file==0) {
				LOG(L_ERR, "ERROR: fifo_server: trimming filename\n");
				goto consume;
			}
		}
		/* make command zero-terminated */
		*file_sep=0;

		f=lookup_fifo_cmd( command );
		if (f==0) {
			LOG(L_ERR, "ERROR: fifo_server: command %s is not available\n",
				command);
			goto consume;
		}
		if (f->f(fifo_stream, file)<0) {
			LOG(L_ERR, "ERROR: fifo_server: command (%s) "
				"processing failed\n", command );
			goto consume;
		}

consume:
		if (file) { pkg_free(file); file=0;}
		consume_request(fifo_stream);
	}
}

int open_fifo_server()
{
	if (fifo==NULL) {
		DBG("TM: open_uac_fifo: no fifo will be opened\n");
		/* everything is ok, we just do not want to start */
		return 1;
	}
	DBG("TM: open_uac_fifo: opening fifo...\n");
	if ((mkfifo(fifo, fifo_mode)<0) && (errno!=EEXIST)) {
		LOG(L_ERR, "ERROR: open_fifo_server; can't create FIFO: %s\n",
			strerror(errno));
		return -1;
	}
	process_no++;
	fifo_pid=fork();
	if (fifo_pid<0) {
		LOG(L_ERR, "ERROR: open_fifo_server: failure to fork: %s\n",
			strerror(errno));
		return -1;
	}
	if (fifo_pid==0) { /* child == FIFO server */
		LOG(L_INFO, "INFO: fifo process starting: %d\n", getpid());
		fifo_read=open(fifo, O_RDONLY, 0);
		if (fifo_read<0) {
			LOG(L_ERR, "SER: open_uac_fifo: fifo_read did not open: %s\n",
				strerror(errno));
			return -1;
		}
		fifo_stream=fdopen(fifo_read, "r"	);
		if (fifo_stream==NULL) {
			LOG(L_ERR, "SER: open_uac_fifo: fdopen failed: %s\n",
				strerror(errno));
			return -1;
		}
		LOG(L_INFO, "SER: open_uac_fifo: fifo server up at %s...\n",
			fifo);
		fifo_server( fifo_stream ); /* never retruns */
	}
	/* dad process */
	pids[process_no]=fifo_pid;
	/* make sure the read fifo will not close */
	fifo_write=open(fifo, O_WRONLY, 0);
	if (fifo_write<0) {
		LOG(L_ERR, "SER: open_uac_fifo: fifo_write did not open: %s\n",
			strerror(errno));
		return -1;
	}
	return 1;
}

/* diagnostic and hello-world FIFO command */
int print_fifo_cmd( FILE *stream, char *response_file )
{
	char text[MAX_PRINT_TEXT];
	int text_len;
	int file;
	
	/* expect one line which will be printed out */
	if (!read_line(text, MAX_PRINT_TEXT, stream, &text_len)) {
		LOG(L_ERR, "ERROR: print_fifo_cmd: too big text\n");
		return -1;
	}
	/* now the work begins */
	if (response_file) {
		file=open( response_file , O_WRONLY);
		if (file<0) {
			LOG(L_ERR, "ERROR: print_fifo_cmd: open error (%s): %s\n",
				response_file, strerror(errno));
			return -1;
		}
		if (write(file, text,text_len)<0) {
			LOG(L_ERR, "ERROR: print_fifo_cmd: write error: %s\n",
				 strerror(errno));
			close(file);
			return -1;
		}
		close(file);
	} else {
		LOG(L_INFO, "INFO: print_fifo_cmd: %.*s\n", 
			text_len, text );
	}
	return 1;
}
