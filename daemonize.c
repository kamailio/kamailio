/*
 * $Id$
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
/*
 * 
 * History:
 * --------
 *  2004-02-20  removed from ser main.c into its own file (andrei)
 *  2004-03-04  moved setuid/setgid in do_suid() (andrei)
 *  2004-03-25  added increase_open_fds & set_core_dump (andrei)
 *  2004-05-03  applied pgid patch from janakj
 */

#define _XOPEN_SOURCE   /* needed on linux for the  getpgid prototype */
#define _XOPEN_SOURCE_EXTENDED /* same as above */
#define __EXTENSIONS__ /* needed on solaris: if XOPEN_SOURCE is defined
                          struct timeval defintion from <sys/time.h> won't
                          be included => workarround define _EXTENSIONS_
                           -andrei */

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>      /* setrlimit */
#include <sys/resource.h> /* setrlimit */

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

	/* flush std file descriptors to avoid flushes after fork
	 *  (same message appearing multiple times)
	 *  and switch to unbuffered
	 */
	setbuf(stdout, 0);
	setbuf(stderr, 0);
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

	if (pgid_file!=0){
		if ((pid_stream=fopen(pgid_file, "r"))!=NULL){
			fscanf(pid_stream, "%d", &p);
			fclose(pid_stream);
			if (p==-1){
				LOG(L_CRIT, "pgid file %s exists, but doesn't contain a valid"
				    " pgid number\n", pgid_file);
				goto error;
			}
		}
		if (own_pgid){
			pid=getpgid(0);
			if ((pid_stream=fopen(pgid_file, "w"))==NULL){
				LOG(L_WARN, "unable to create pgid file %s: %s\n",
					pgid_file, strerror(errno));
				goto error;
			}else{
				fprintf(pid_stream, "%i\n", (int)pid);
				fclose(pid_stream);
			}
		}else{
			LOG(L_WARN, "we don't have our own process so we won't save"
					" our pgid\n");
			unlink(pgid_file); /* just to be sure nobody will miss-use the old
								  value*/
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
	if (gid){
		if(setgid(gid)<0){
			LOG(L_CRIT, "cannot change gid to %d: %s\n", gid, strerror(errno));
			goto error;
		}
	}
	
	if(uid){
		if(setuid(uid)<0){
			LOG(L_CRIT, "cannot change uid to %d: %s\n", uid, strerror(errno));
			goto error;
		}
	}
	return 0;
error:
	return -1;
}



/* try to increase the open file limit */
int increase_open_fds(int target)
{
	struct rlimit lim;
	struct rlimit orig;
	
	if (getrlimit(RLIMIT_NOFILE, &lim)<0){
		LOG(L_CRIT, "cannot get the maximum number of file descriptors: %s\n",
				strerror(errno));
		goto error;
	}
	orig=lim;
	DBG("current open file limits: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if ((lim.rlim_cur==RLIM_INFINITY) || (target<=lim.rlim_cur))
		/* nothing to do */
		goto done;
	else if ((lim.rlim_max==RLIM_INFINITY) || (target<=lim.rlim_max)){
		lim.rlim_cur=target; /* increase soft limit to target */
	}else{
		/* more than the hard limit */
		LOG(L_INFO, "trying to increase the open file limit"
				" past the hard limit (%ld -> %d)\n", 
				(unsigned long)lim.rlim_max, target);
		lim.rlim_max=target;
		lim.rlim_cur=target;
	}
	DBG("increasing open file limits to: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if (setrlimit(RLIMIT_NOFILE, &lim)<0){
		LOG(L_CRIT, "cannot increase the open file limit to"
				" %lu/%lu: %s\n",
				(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max,
				strerror(errno));
		if (orig.rlim_max>orig.rlim_cur){
			/* try to increase to previous maximum, better than not increasing
		 	* at all */
			lim.rlim_max=orig.rlim_max;
			lim.rlim_cur=orig.rlim_max;
			if (setrlimit(RLIMIT_NOFILE, &lim)==0){
				LOG(L_CRIT, " maximum number of file descriptors increased to"
						" %u\n",(unsigned)orig.rlim_max);
			}
		}
		goto error;
	}
done:
	return 0;
error:
	return -1;
}



/* enable core dumps */
int set_core_dump(int enable, int size)
{
	struct rlimit lim;
	struct rlimit newlim;
	
	if (enable){
		if (getrlimit(RLIMIT_CORE, &lim)<0){
			LOG(L_CRIT, "cannot get the maximum core size: %s\n",
					strerror(errno));
			goto error;
		}
		if (lim.rlim_cur<size){
			/* first try max limits */
			newlim.rlim_max=RLIM_INFINITY;
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)==0) goto done;
			/* now try with size */
			if (lim.rlim_max<size){
				newlim.rlim_max=size;
			}
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)==0) goto done;
			/* if this failed too, try rlim_max, better than nothing */
			newlim.rlim_max=lim.rlim_max;
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)<0){
				LOG(L_CRIT, "could increase core limits at all: %s\n",
						strerror (errno));
			}else{
				LOG(L_CRIT, "core limits increased only to %lu\n",
						(unsigned long)lim.rlim_max);
			}
			goto error; /* it's an error we haven't got the size we wanted*/
		}
		goto done; /*nothing to do */
	}else{
		/* disable */
		newlim.rlim_cur=0;
		newlim.rlim_max=0;
		if (setrlimit(RLIMIT_CORE, &newlim)<0){
			LOG(L_CRIT, "failed to disable core dumps: %s\n",
					strerror(errno));
			goto error;
		}
	}
done:
	DBG("core dump limits set to %lu\n", (unsigned long)newlim.rlim_cur);
	return 0;
error:
	return -1;
}
