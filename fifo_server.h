/*
 * $Id$
 *
 */

#ifndef _FIFO_SERVER_H
#define _FIFO_SERVER_H

#include <stdio.h>

#define CMD_SEPARATOR ':'

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

int open_fifo_server();

int print_fifo_cmd( FILE *stream, char *response_file );
#endif
