/*
 * $Id$
 *
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
 */


#ifndef _FIFO_SERVER_H
#define _FIFO_SERVER_H

#include <stdio.h>

#define CMD_SEPARATOR ':'

/* core FIFO command set */
/* echo input */
#define FIFO_PRINT "print"
/* print server's uptime */
#define FIFO_UPTIME "uptime"
/* print server's version */
#define FIFO_VERSION "version"
/* print available FIFO commands */
#define FIFO_WHICH "which"
/* print server's process table */
#define FIFO_PS "ps"
/* print server's command line arguments */
#define FIFO_ARG "arg"
/* print server's working directory */
#define FIFO_PWD "pwd"
/* kill the server */
#define FIFO_KILL "kill"

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
