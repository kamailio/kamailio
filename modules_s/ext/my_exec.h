/*
 *
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



#ifndef _MY_POPEN_H
#define _MY_POPEN_H

#include <sys/types.h>


struct program
{
	int   fd_in;
	int   fd_out;
	pid_t pid;
	int   stat;
};


int init_ext(int rank);
int start_prog( char *cmd );
int kill_prog();

extern struct program _private_prog;


#ifndef _MY_POPEN_NO_INLINE
inline int sendto_prog(char *in, int in_len, int end_it)
{
	int foo;

	if (!_private_prog.pid)
		return -1;
	foo = write( _private_prog.fd_in, in, in_len);
	if (end_it)
		close( _private_prog.fd_in);
	return foo;
}



inline int recvfrom_prog(char *out, int out_len)
{
	return read( _private_prog.fd_out, out, out_len);
}



inline int is_finished()
{
	return ( (_private_prog.pid)?-1:_private_prog.stat );
}



inline int wait_prog()
{
	int n;
	char c;

	if (!_private_prog.pid)
		return -1;
	while ( (n=read( _private_prog.fd_out, &c, 1))!=0)
		if (n==-1) return -1;
	return 0;
}



inline void close_prog_input()
{
	close(_private_prog.fd_in);
}



inline void close_prog_output()
{
	close(_private_prog.fd_out);
}


#endif
#endif
