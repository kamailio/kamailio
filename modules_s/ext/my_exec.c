/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
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



#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#define  _MY_POPEN_NO_INLINE

#include "../../dprint.h"
#include "../../error.h"
#include "config.h"

#include "my_exec.h"

struct program _private_prog;


static void sig_chld(int signo)
{
	int status;

	DBG("DEBUG: child left\n");

	if ( !_private_prog.pid)
		/*is not ours */
		return;

	while(waitpid(_private_prog.pid, &(_private_prog.stat), 0)<0) {
		LOG(L_ERR, "ERROR: waitpid failed, errno=%d\n", errno );
		if (errno==EINTR)
			continue;
		else
			return;
	}

	if (WIFEXITED(_private_prog.stat)) {
		status=WEXITSTATUS(_private_prog.stat);
		DBG("DEBUG: child terminated with status %d\n", status );
		if (status!=0) {
			LOG(L_ERR, "ERROR: child terminated, status=%d \n", status);
		}
	} else {
       	LOG(L_ERR, "ERROR: child terminated abruptly; "
           	"signaled=%d,stopped=%d\n",
           	WIFSIGNALED(_private_prog.stat),
           	WIFSTOPPED(_private_prog.stat));
   	}

	_private_prog.pid = 0;
	_private_prog.fd_in = 0;
	_private_prog.fd_in = 0;

}



int init_ext(int rank)
{
	if (rank > 0) {
		if (signal(SIGCHLD,sig_chld)==SIG_ERR)
			return -1;
		
		_private_prog.pid = 0;
		_private_prog.stat = 0;
	}
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
	if (pipe(p_in)<0) {
		LOG(L_ERR, "ERROR: start_prog: open(pipe_in) failed\n");
		return -1;
	}
	if (pipe(p_out)<0) {
		LOG(L_ERR, "ERROR: start_prog: open(pipe_out) failed\n");
		return -1;
	}

	/* let's fork */
	if ((pid=fork())<0 ) {
		LOG(L_ERR, "ERROR: start_prog: forking failed\n");
		return -1;
	}

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
		execl(SHELL,SHELL_NAME,SHELL_PARAM,cmd,0);
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


