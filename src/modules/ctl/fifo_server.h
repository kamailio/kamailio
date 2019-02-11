/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef _FIFO_SERVER_H
#define _FIFO_SERVER_H

#include <stdio.h>

#define CMD_SEPARATOR ':'

extern char* fifo_dir;
extern int   fifo_reply_retries;
extern int   fifo_reply_wait;

/* Initialize FIFO server data structures */
int init_fifo_fd(char* fifo, int fifo_mode, int fifo_uid, int fifo_gid,
					int* wfd);

int fifo_process(char* msg_buf, int size, int* bytes_need, void *sh, void** s);
/* memory deallocation */
void destroy_fifo(int read_fd, int w_fd, char* fname);
int fifo_rpc_init();

#endif
