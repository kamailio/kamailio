#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#define  _MY_POPEN_NO_INLINE
#include "my_exec.h"




static void sig_chld(int signo)
{
	printf("We have a dead child\n");

	if ( !_private_prog.pid)
		/*is not ours */
		return;

	while(waitpid(_private_prog.pid, &(_private_prog.stat), 0)<0)
		if (errno!=EINTR)
			return;

	_private_prog.pid = 0;
	_private_prog.fd_in = 0;
	_private_prog.fd_in = 0;
}



int init_ext()
{
	if (signal(SIGCHLD,sig_chld)==SIG_ERR)
		return -1;

	_private_prog.pid = 0;
	_private_prog.stat = 0;
	return 1;
}




int start_prog( char *cmd )
{
	int   p_in[2];
	int   p_out[2];
	pid_t pid;

	if (_private_prog.pid)
		return -1;

	/* pipes for std_in and std_out */
	if (pipe(p_in)<0)
		return -1;
	if (pipe(p_out)<0)
		return -1;

	/* let's fork */
	if ((pid=fork())<0 )
		return -1;

	if (pid) {
		/* parent */
		close( p_in[0] );
		close( p_out[1]);
		_private_prog.fd_in = p_in[1];
		_private_prog.fd_out= p_out[0];
		_private_prog.pid   = pid;
		return 0;
	} else {
		/* child */
		close( p_in[1] );
		if (p_in[0]!=STDIN_FILENO) {
			dup2(p_in[0],STDIN_FILENO);
			close(p_in[0]);
		}
		close( p_out[0] );
		if (p_out[1]!=STDOUT_FILENO) {
			dup2(p_out[1],STDOUT_FILENO);
			close(p_out[1]);
		}
		execl("/bin/bash","bash","-c",cmd,0);
		_exit(127);
	}

	return 0;
}




int kill_prog()
{
	if (!_private_prog.pid)
		/* nothing to kill */
		return -1;

	kill(_private_prog.pid,SIGKILL);
	return 0;
}


