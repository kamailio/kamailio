/*
 *
 * $Id$
 *
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


int init_ext();
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
