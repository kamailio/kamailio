/*
 * $Id$
 *
 */

#ifndef _FIFO_SERVER_H
#define _FIFO_SERVER_H

#include <stdio.h>

#define CMD_SEPARATOR ':'

/* core FIFO command set */
#define FIFO_PRINT "print"
#define FIFO_UPTIME "uptime"
#define FIFO_VERSION "version"
#define FIFO_WHICH "which"

#define MAX_CTIME_LEN 128

typedef int (fifo_cmd)( FILE *fifo_stream, char *response_file );

struct fifo_command{
	fifo_cmd *f;
	struct fifo_command *next;
	void *param;
	char *name;
};

int register_fifo_cmd(fifo_cmd f, char *cmd_name, void *param);

/* read a single EoL-terminated line from fifo */
int read_line( char *b, int max, FILE *stream, int *read );
/* consume EoL from fifo */
int read_eol( FILE *stream );
/* consume a set of EoL-terminated lines terminated by an additional EoL */
int read_line_set(char *buf, int max_len, FILE *fifo, int *len);
/* consume a set of EoL-terminated lines terminated by a single dot line */
int read_body(char *buf, int max_len, FILE *fifo, int *len);

int open_fifo_server();

/* regsiter core FIFO command set */
int register_core_fifo();

FILE *open_reply_pipe( char *pipe_name );

/* tell FIFO client an error occured via reply pipe */
void fifo_reply( char *reply_fifo, char *reply_fmt, ... );


#endif
