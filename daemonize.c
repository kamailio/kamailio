/*
 * $Id$
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
/*
 * 
 * History:
 * --------
 *  2004-02-20  removed from ser main.c into its own file (andrei)
 *  2004-03-04  moved setuid/setgid in do_suid() (andrei)
 */

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "daemonize.h"
#include "globals.h"
#include "dprint.h"


#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */



/* daemon init, return 0 on success, -1 on error */
int daemonize(char*  name)
{
	FILE *pid_stream;
	pid_t pid;
	int r, p;


	p=-1;


	if (chroot_dir&&(chroot(chroot_dir)<0)){
		LOG(L_CRIT, "Cannot chroot to %s: %s\n", chroot_dir, strerror(errno));
		goto error;
	}
	
	if (chdir(working_dir)<0){
		LOG(L_CRIT,"cannot chdir to %s: %s\n", working_dir, strerror(errno));
		goto error;
	}

	/* fork to become!= group leader*/
	if ((pid=fork())<0){
		LOG(L_CRIT, "Cannot fork:%s\n", strerror(errno));
		goto error;
	}else if (pid!=0){
		/* parent process => exit*/
		exit(0);
	}
	/* become session leader to drop the ctrl. terminal */
	if (setsid()<0){
		LOG(L_WARN, "setsid failed: %s\n",strerror(errno));
	}else{
		own_pgid=1;/* we have our own process group */
	}
	/* fork again to drop group  leadership */
	if ((pid=fork())<0){
		LOG(L_CRIT, "Cannot  fork:%s\n", strerror(errno));
		goto error;
	}else if (pid!=0){
		/*parent process => exit */
		exit(0);
	}

	/* added by noh: create a pid file for the main process */
	if (pid_file!=0){
		
		if ((pid_stream=fopen(pid_file, "r"))!=NULL){
			fscanf(pid_stream, "%d", &p);
			fclose(pid_stream);
			if (p==-1){
				LOG(L_CRIT, "pid file %s exists, but doesn't contain a valid"
					" pid number\n", pid_file);
				goto error;
			}
			if (kill((pid_t)p, 0)==0 || errno==EPERM){
				LOG(L_CRIT, "running process found in the pid file %s\n",
					pid_file);
				goto error;
			}else{
				LOG(L_WARN, "pid file contains old pid, replacing pid\n");
			}
		}
		pid=getpid();
		if ((pid_stream=fopen(pid_file, "w"))==NULL){
			LOG(L_WARN, "unable to create pid file %s: %s\n", 
				pid_file, strerror(errno));
			goto error;
		}else{
			fprintf(pid_stream, "%i\n", (int)pid);
			fclose(pid_stream);
		}
	}
	
	/* try to replace stdin, stdout & stderr with /dev/null */
	if (freopen("/dev/null", "r", stdin)==0){
		LOG(L_ERR, "unable to replace stdin with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	if (freopen("/dev/null", "w", stdout)==0){
		LOG(L_ERR, "unable to replace stdout with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	/* close stderr only if log_stderr=0 */
	if ((!log_stderr) &&(freopen("/dev/null", "w", stderr)==0)){
		LOG(L_ERR, "unable to replace stderr with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	
	/* close any open file descriptors */
	closelog();
	for (r=3;r<MAX_FD; r++){
			close(r);
	}
	
	if (log_stderr==0)
		openlog(name, LOG_PID|LOG_CONS, log_facility);
		/* LOG_CONS, LOG_PERRROR ? */

	return  0;

error:
	return -1;
}



int do_suid()
{
	if (gid&&(setgid(gid)<0)){
		LOG(L_CRIT, "cannot change gid to %d: %s\n", gid, strerror(errno));
		goto error;
	}
	
	if(uid&&(setuid(uid)<0)){
		LOG(L_CRIT, "cannot change uid to %d: %s\n", uid, strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}


