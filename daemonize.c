/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*!
 * \file
 * \brief Kamailio core :: Daemon init
 * \ingroup core
 * Module: \ref core
 */



#include <sys/types.h>

#define _XOPEN_SOURCE   /*!< needed on linux for the  getpgid prototype,  but
                           openbsd 3.2 won't include common types (uint a.s.o)
                           if defined before including sys/types.h */
#define _XOPEN_SOURCE_EXTENDED /*!< same as \ref _XOPEN_SOURCE */
#define __USE_XOPEN_EXTENDED /*!< same as \ref _XOPEN_SOURCE, overrides features.h */
#define __EXTENSIONS__ /*!< needed on solaris: if XOPEN_SOURCE is defined
                          struct timeval defintion from <sys/time.h> won't
                          be included => workarround define _EXTENSIONS_
                           -andrei */
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>    
#include <sys/resource.h> /* setrlimit */
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#ifdef __OS_linux
#include <sys/prctl.h>
#endif

#ifdef HAVE_SCHED_SETSCHEDULER
#include <sched.h>
#endif

#ifdef _POSIX_MEMLOCK
#define HAVE_MLOCKALL
#include <sys/mman.h>
#endif

#include "daemonize.h"
#include "globals.h"
#include "dprint.h"
#include "signals.h"
#include "cfg/cfg.h"


#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */

/** temporary pipe FDs for sending exit status back to the ancestor process.
 * This pipe is used to send the desired exit status to the initial process,
 * that waits for it in the foreground. This way late errors preventing
 * startup (e.g. during modules child inits or TCP late init) can still be
 * reported back.
 */
static int daemon_status_fd[2];



/** init daemon status reporting.
 * Must be called before any other daemon_status function has a chance to
 * run.
 */
void daemon_status_init()
{
	daemon_status_fd[0] = -1;
	daemon_status_fd[1] = -1;
}



/** pre-daemonize init for daemon status reporting.
 * Must be called before forking.
 * Typically the parent process will call daemon_status_wait() while
 * one of the children will call daemon_status_send() at some point.
 *
 * @return 0 on success, -1 on error (and sets errno).
 */
int daemon_status_pre_daemonize(void)
{
	int ret;
	
retry:
	ret = pipe(daemon_status_fd);
	if (ret < 0 && errno == EINTR)
		goto retry;
	return ret;
}



/** wait for an exit status to be send by daemon_status_send().
 * @param status - filled with the sent status (a char).
 * @return  0 on success, -1 on error (e.g. process died before sending
 *          status, not intialized a.s.o.).
 * Side-effects: it will close the write side of the pipe
 *  (must not be used from the same process as the daemon_status_send()).
 * Note: if init is not complete (only init, but no pre-daemonize)
 * it will return success always and status 0.
 */
int daemon_status_wait(char* status)
{
	int ret;
	
	/* close the output side of the pipe */
	if (daemon_status_fd[1] != -1) {
		close(daemon_status_fd[1]);
		daemon_status_fd[1] = -1;
	}
	if (daemon_status_fd[0] == -1) {
		*status = 0;
		return -1;
	}
retry:
	ret = read(daemon_status_fd[0], status, 1);
	if (ret < 0 && errno == EINTR)
		goto retry;
	return (ret ==1 ) ? 0 : -1;
}



/** send 'status' to a waiting process running daemon_status_wait().
 * @param status - status byte
 * @return 0 on success, -1 on error.
 * Note: if init is not complete (only init, but no pre-daemonize)
 * it will return success always.
 */
int daemon_status_send(char status)
{
	int ret;

	if (daemon_status_fd[1] == -1)
		return 0;
retry:
	ret = write(daemon_status_fd[1], &status, 1);
	if (ret < 0 && errno == EINTR)
		goto retry;
	return (ret ==1 ) ? 0 : -1;
}



/** cleanup functions for new processes.
 * Should be called after fork(), for each new process that _does_ _not_
 * use  daemon_status_send() or daemon_status_wait().
 */
void daemon_status_on_fork_cleanup()
{
	if (daemon_status_fd[0] != -1) {
		close(daemon_status_fd[0]);
		daemon_status_fd[0] = -1;
	}
	if (daemon_status_fd[1] != -1) {
		close(daemon_status_fd[1]);
		daemon_status_fd[1] = -1;
	}
}



/** cleanup functions for processes that don't intead to wait.
 * Should be called after fork(), for each new process that doesn't
 * use daemon_status_wait().
 */
void daemon_status_no_wait()
{
	if (daemon_status_fd[0] != -1) {
		close(daemon_status_fd[0]);
		daemon_status_fd[0] = -1;
	}
}


/**
 * enable dumpable flag for core dumping after setuid() & friends
 * @return 0 when no critical error occured, -1 on such error
 */
int enable_dumpable(void)
{
#ifdef __OS_linux
	struct rlimit lim;
	/* re-enable core dumping on linux after setuid() & friends */
	if(disable_core_dump==0) {
		LM_DBG("trying enable core dumping...\n");
		if(prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)<=0) {
			LM_DBG("core dumping is disabled now...\n");
			if(prctl(PR_SET_DUMPABLE, 1, 0, 0, 0)<0) {
				LM_WARN("cannot re-enable core dumping!\n");
			} else {
				LM_DBG("core dumping has just been enabled...\n");
				if (getrlimit(RLIMIT_CORE, &lim)<0){
					LM_CRIT( "cannot get the maximum core size: %s\n",
							strerror(errno));
					return -1;
				} else {
					LM_DBG("current core file limit: %lu (max: %lu)\n",
						(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
				}
			}
		} else {
			LM_DBG("core dumping is enabled now (%d)...\n",
					prctl(PR_GET_DUMPABLE, 0, 0, 0, 0));
		}
	}
#endif
	return 0;
}

/** daemon init.
 *@param name - daemon name used for logging (used when opening syslog).
 *@param status_wait  - if 1 the original process will wait until it gets
 *                  an exit code send using daemon_status_send().
 *@return 0 in the child process (in case of daemonize mode),
 *        -1 on error.
 * The original process that called daemonize() will be terminated if
 * dont_daemonize == 0. The exit code depends on status_wait. If status_wait
 * is non-zero, the original process will wait for a status code, that
 * must be sent with daemon_status_send() (daemon_status_send() must be
 * called or the original process will remain waiting until all the children
 * close()). If status_wait is 0, the original process will exit immediately
 * with exit(0).
 * Global variables/config params used:
 * dont_daemonize
 * chroot_dir
 * working_dir
 * pid_file - if set the pid will be written here (ascii).
 * pgid_file - if set, the pgid will be written here (ascii).
 * log_stderr - if not set syslog will be opened (openlog(name,...))
 * 
 *
 * Side-effects:
 *  sets own_pgid after becoming session leader (own process group).
*/
int daemonize(char*  name,  int status_wait)
{
	FILE *pid_stream;
	pid_t pid;
	int r, p;
	char pipe_status;
	uid_t pid_uid;
	gid_t pid_gid;

	if(uid) pid_uid = uid;
	else pid_uid = -1;

	if(gid) pid_gid = gid;
	else pid_gid = -1;

	p=-1;
	/* flush std file descriptors to avoid flushes after fork
	 *  (same message appearing multiple times)
	 *  and switch to unbuffered
	 */
	setbuf(stdout, 0);
	setbuf(stderr, 0);
	if (chroot_dir&&(chroot(chroot_dir)<0)){
		LM_CRIT("Cannot chroot to %s: %s\n", chroot_dir, strerror(errno));
		goto error;
	}
	
	if (chdir(working_dir)<0){
		LM_CRIT("cannot chdir to %s: %s\n", working_dir, strerror(errno));
		goto error;
	}

	if (!dont_daemonize) {
		if (status_wait) {
			if (daemon_status_pre_daemonize() < 0)
				goto error;
		}
		/* fork to become!= group leader*/
		if ((pid=fork())<0){
			LM_CRIT("Cannot fork:%s\n", strerror(errno));
			goto error;
		}else if (pid!=0){
			if (status_wait) {
				if (daemon_status_wait(&pipe_status) == 0)
					exit((int)pipe_status);
				else{
					LM_ERR("Main process exited before writing to pipe\n");
					exit(-1);
				}
			}
			exit(0);
		}
		if (status_wait)
			daemon_status_no_wait(); /* clean unused read fd */
		/* become session leader to drop the ctrl. terminal */
		if (setsid()<0){
			LM_WARN("setsid failed: %s\n",strerror(errno));
		}else{
			own_pgid=1;/* we have our own process group */
		}
		/* fork again to drop group  leadership */
		if ((pid=fork())<0){
			LM_CRIT("Cannot  fork:%s\n", strerror(errno));
			goto error;
		}else if (pid!=0){
			/*parent process => exit */
			exit(0);
		}
	}

	if(enable_dumpable()<0)
		goto error;

	/* added by noh: create a pid file for the main process */
	if (pid_file!=0){
		
		if ((pid_stream=fopen(pid_file, "r"))!=NULL){
			if (fscanf(pid_stream, "%d", &p) < 0) {
				LM_WARN("could not parse pid file %s\n", pid_file);
			}
			fclose(pid_stream);
			if (p==-1){
				LM_CRIT("pid file %s exists, but doesn't contain a valid"
					" pid number\n", pid_file);
				goto error;
			}
			if (kill((pid_t)p, 0)==0 || errno==EPERM){
				LM_CRIT("running process found in the pid file %s\n",
					pid_file);
				goto error;
			}else{
				LM_WARN("pid file contains old pid, replacing pid\n");
			}
		}
		pid=getpid();
		if ((pid_stream=fopen(pid_file, "w"))==NULL){
			LM_WARN("unable to create pid file %s: %s\n", 
				pid_file, strerror(errno));
			goto error;
		}else{
			fprintf(pid_stream, "%i\n", (int)pid);
			fclose(pid_stream);
			if(chown(pid_file, pid_uid, pid_gid)<0) {
				LM_ERR("failed to chwon PID file: %s\n", strerror(errno));
				goto error;
			}
		}
	}

	if (pgid_file!=0){
		if ((pid_stream=fopen(pgid_file, "r"))!=NULL){
			if (fscanf(pid_stream, "%d", &p) < 0) {
				 LM_WARN("could not parse pgid file %s\n", pgid_file);
			}
			fclose(pid_stream);
			if (p==-1){
				LM_CRIT("pgid file %s exists, but doesn't contain a valid"
				    " pgid number\n", pgid_file);
				goto error;
			}
		}
		if (own_pgid){
			pid=getpgid(0);
			if ((pid_stream=fopen(pgid_file, "w"))==NULL){
				LM_WARN("unable to create pgid file %s: %s\n",
					pgid_file, strerror(errno));
				goto error;
			}else{
				fprintf(pid_stream, "%i\n", (int)pid);
				fclose(pid_stream);
				if(chown(pid_file, pid_uid, pid_gid)<0) {
					LM_ERR("failed to chwon PGID file: %s\n", strerror(errno));
					goto error;
				}
			}
		}else{
			LM_WARN("we don't have our own process so we won't save"
					" our pgid\n");
			unlink(pgid_file); /* just to be sure nobody will miss-use the old
								  value*/
		}
	}
	
	/* try to replace stdin, stdout & stderr with /dev/null */
	if (freopen("/dev/null", "r", stdin)==0){
		LM_ERR("unable to replace stdin with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	if (freopen("/dev/null", "w", stdout)==0){
		LM_ERR("unable to replace stdout with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	/* close stderr only if log_stderr=0 */
	if ((!log_stderr) &&(freopen("/dev/null", "w", stderr)==0)){
		LM_ERR("unable to replace stderr with /dev/null: %s\n",
				strerror(errno));
		/* continue, leave it open */
	};
	
	/* close all but the daemon_status_fd output as the main process
	  must still write into it to tell the parent to exit with 0 */
	closelog();
	for (r=3;r<MAX_FD; r++){
		if(r !=  daemon_status_fd[1])
			close(r);
	}
	
	if (log_stderr==0)
		openlog(name, LOG_PID|LOG_CONS, cfg_get(core, core_cfg, log_facility));
		/* LOG_CONS, LOG_PERRROR ? */

	return  0;

error:
	return -1;
}



int do_suid()
{
	struct passwd *pw;
	
	if (gid){
		if(gid!=getgid()) {
			if(setgid(gid)<0){
				LM_CRIT("cannot change gid to %d: %s\n", gid, strerror(errno));
				goto error;
			}
		}
	}
	
	if(uid){
		if (!(pw = getpwuid(uid))){
			LM_CRIT("user lookup failed: %s\n", strerror(errno));
			goto error;
		}
		if(uid!=getuid()) {
			if(initgroups(pw->pw_name, pw->pw_gid)<0){
				LM_CRIT("cannot set supplementary groups: %s\n", 
							strerror(errno));
				goto error;
			}
			if(setuid(uid)<0){
				LM_CRIT("cannot change uid to %d: %s\n", uid, strerror(errno));
				goto error;
			}
		}
	}

	if(enable_dumpable()<0)
		goto error;

	return 0;
error:
	return -1;
}



/*! \brief try to increase the open file limit */
int increase_open_fds(int target)
{
	struct rlimit lim;
	struct rlimit orig;
	
	if (getrlimit(RLIMIT_NOFILE, &lim)<0){
		LM_CRIT("cannot get the maximum number of file descriptors: %s\n",
				strerror(errno));
		goto error;
	}
	orig=lim;
	LM_DBG("current open file limits: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if ((lim.rlim_cur==RLIM_INFINITY) || (target<=lim.rlim_cur))
		/* nothing to do */
		goto done;
	else if ((lim.rlim_max==RLIM_INFINITY) || (target<=lim.rlim_max)){
		lim.rlim_cur=target; /* increase soft limit to target */
	}else{
		/* more than the hard limit */
		LM_INFO("trying to increase the open file limit"
				" past the hard limit (%ld -> %d)\n", 
				(unsigned long)lim.rlim_max, target);
		lim.rlim_max=target;
		lim.rlim_cur=target;
	}
	LM_DBG("increasing open file limits to: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if (setrlimit(RLIMIT_NOFILE, &lim)<0){
		LM_CRIT("cannot increase the open file limit to"
				" %lu/%lu: %s\n",
				(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max,
				strerror(errno));
		if (orig.rlim_max>orig.rlim_cur){
			/* try to increase to previous maximum, better than not increasing
		 	* at all */
			lim.rlim_max=orig.rlim_max;
			lim.rlim_cur=orig.rlim_max;
			if (setrlimit(RLIMIT_NOFILE, &lim)==0){
				LM_CRIT(" maximum number of file descriptors increased to"
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



/*! \brief enable core dumps */
int set_core_dump(int enable, long unsigned int size)
{
	struct rlimit lim;
	struct rlimit newlim;
	
	if (enable){
		if (getrlimit(RLIMIT_CORE, &lim)<0){
			LM_CRIT("cannot get the maximum core size: %s\n",
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
				LM_CRIT("could increase core limits at all: %s\n",
						strerror (errno));
			}else{
				LM_CRIT("core limits increased only to %lu\n",
						(unsigned long)lim.rlim_max);
			}
			goto error; /* it's an error we haven't got the size we wanted*/
		}else{
			newlim.rlim_cur=lim.rlim_cur;
			newlim.rlim_max=lim.rlim_max;
			goto done; /*nothing to do */
		}
	}else{
		/* disable */
		newlim.rlim_cur=0;
		newlim.rlim_max=0;
		if (setrlimit(RLIMIT_CORE, &newlim)<0){
			LM_CRIT("failed to disable core dumps: %s\n",
					strerror(errno));
			goto error;
		}
	}
done:
	LM_DBG("core dump limits set to %lu\n", (unsigned long)newlim.rlim_cur);
	return 0;
error:
	return -1;
}



/*! \brief lock pages in memory (make the process not swapable) */
int mem_lock_pages()
{
#ifdef HAVE_MLOCKALL
	if (mlockall(MCL_CURRENT|MCL_FUTURE) !=0){
		LM_WARN("failed to lock the memory pages (disable swap): %s [%d]\n",
				strerror(errno), errno);
		goto error;
	}
	return 0;
error:
	return -1;
#else /* if MLOCKALL not defined return error */
		LM_WARN("failed to lock the memory pages: no mlockall support\n");
	return -1;
#endif 
}


/*! \brief tries to set real time priority 
 * policy: 0 - SCHED_OTHER, 1 - SCHED_RR, 2 - SCHED_FIFO */
int set_rt_prio(int prio, int policy)
{
#ifdef HAVE_SCHED_SETSCHEDULER
	struct sched_param sch_p;
	int min_prio, max_prio;
	int sched_policy;
	
	switch(policy){
		case 0:
			sched_policy=SCHED_OTHER;
			break;
		case 1:
			sched_policy=SCHED_RR;
			break;
		case 2:
			sched_policy=SCHED_FIFO;
			break;
		default:
			LM_WARN("invalid scheduling policy,using SCHED_OTHER\n");
			sched_policy=SCHED_OTHER;
	}
	memset(&sch_p, 0, sizeof(sch_p));
	max_prio=sched_get_priority_max(policy);
	min_prio=sched_get_priority_min(policy);
	if (prio<min_prio){
		LM_WARN("scheduling priority %d too small, using minimum value"
					" (%d)\n", prio, min_prio);
		prio=min_prio;
	}else if (prio>max_prio){
		LM_WARN("scheduling priority %d too big, using maximum value"
					" (%d)\n", prio, max_prio);
		prio=max_prio;
	}
	sch_p.sched_priority=prio;
	if (sched_setscheduler(0, sched_policy, &sch_p) != 0){
		LM_WARN("could not switch to real time priority: %s [%d]\n",
					strerror(errno), errno);
		return -1;
	};
	return 0;
#else
	LM_WARN("real time support not available\n");
	return -1;
#endif
}
