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
 *
 * History:
 * -------
 *  2002-01-29  argc/argv globalized via my_{argc|argv} (jiri)
 *  2003-01-23  mhomed added (jiri)
 *  2003-03-19  replaced all malloc/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-29  pkg cleaners for fifo and script callbacks introduced (jiri)
 *  2003-03-31  removed snmp part (obsolete & no place in core) (andrei)
 *  2003-04-06  child_init called in all processes (janakj)
 *  2003-04-08  init_mallocs split into init_{pkg,shm}_mallocs and 
 *               init_shm_mallocs called after cmd. line parsing (andrei)
 *  2003-04-15  added tcp_disable support (andrei)
 *  2003-05-09  closelog() before openlog to force opening a new fd (needed on solaris) (andrei)
 *  2003-06-11  moved all signal handlers init. in install_sigs and moved it
 *               after daemonize (so that we won't catch anymore our own
 *               SIGCHLD generated when becoming session leader) (andrei)
 *              changed is_main default value to 1 (andrei)
 *  2003-06-28  kill_all_children is now used instead of kill(0, sig)
 *                see comment above it for explanations. (andrei)
 *  2003-06-29  replaced port_no_str snprintf w/ int2str (andrei)
 *  2003-10-10  added switch for config check (-c) (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2004-03-30  core dump is enabled by default
 *              added support for increasing the open files limit    (andrei)
 *  2004-04-28  sock_{user,group,uid,gid,mode} added
 *              user2uid() & user2gid() added  (andrei)
 *  2004-09-11  added timeout on children shutdown and final cleanup
 *               (if it takes more than 60s => something is definitely wrong
 *                => kill all or abort)  (andrei)
 *              force a shm_unlock before cleaning-up, in case we have a
 *               crashed childvwhich still holds the lock  (andrei)
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <net/if.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "config.h"
#include "dprint.h"
#include "daemonize.h"
#include "route.h"
#include "udp_server.h"
#include "globals.h"
#include "mem/mem.h"
#ifdef SHM_MEM
#include "mem/shm_mem.h"
#endif
#include "sr_module.h"
#include "timer.h"
#include "parser/msg_parser.h"
#include "ip_addr.h"
#include "resolve.h"
#include "parser/parse_hname2.h"
#include "parser/digest/digest_parser.h"
#include "fifo_server.h"
#include "unixsock_server.h"
#include "name_alias.h"
#include "hash_func.h"
#include "pt.h"
#include "script_cb.h"
#include "ut.h"
#ifdef USE_TCP
#include "tcp_init.h"
#ifdef USE_TLS
#include "tls/tls_init.h"
#endif
#endif



#include "stats.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif
#include "version.h"

static char id[]="@(#) $Id$";
static char* version=SER_FULL_VERSION;
static char* flags=SER_COMPILE_FLAGS;
char compiled[]= __TIME__ " " __DATE__ ;


static char help_msg[]= "\
Usage: " NAME " -l address [-p port] [-l address [-p port]...] [options]\n\
Options:\n\
    -f file      Configuration file (default " CFG_FILE ")\n\
    -c           Check configuration file for errors\n\
    -p port      Listen on the specified port (default: 5060)\n\
                  applies to the last address in -l and to all \n\
                  following that do not have a corresponding -p\n\
    -l address   Listen on the specified address/interface (multiple -l\n\
                  mean listening on more addresses). The default behavior\n\
                  is to listen on all the interfaces\n\
    -n processes Number of child processes to fork per interface\n\
                  (default: 8)\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                  field to a via\n\
    -R           Same as `-r` but use reverse dns;\n\
                  (to use both use `-rR`)\n\
    -v           Turn on \"via:\" host checking when forwarding replies\n\
    -d           Debugging mode (multiple -d increase the level)\n\
    -D           Do not fork into daemon mode\n\
    -E           Log to stderr\n"
#ifdef USE_TCP
"    -T           Disable tcp\n\
    -N           Number of tcp child processes (default: equal to `-n`)\n"
#endif
"    -V           Version number\n\
    -h           This help message\n\
    -b nr        Maximum receive buffer size which will not be exceeded by\n\
                  auto-probing procedure even if  OS allows\n\
    -m nr        Size of shared memory allocated in Megabytes\n\
    -w dir       Change the working directory to \"dir\" (default \"/\")\n\
    -t dir       Chroot to \"dir\"\n\
    -u uid       Change uid \n\
    -g gid       Change gid \n\
    -P file      Create a pid file\n\
    -G file      Create a pgid file\n\
    -i fifo_path Create a fifo (useful for monitoring " NAME ") \n\
    -x socket    Create a unix domain socket \n"
#ifdef STATS
"    -s file     File to which statistics is dumped (disabled otherwise)\n"
#endif
;

/* print compile-time constants */
void print_ct_constants()
{
#ifdef ADAPTIVE_WAIT
	printf("ADAPTIVE_WAIT_LOOPS=%d, ", ADAPTIVE_WAIT_LOOPS);
#endif
/*
#ifdef SHM_MEM
	printf("SHM_MEM_SIZE=%d, ", SHM_MEM_SIZE);
#endif
*/
	printf("MAX_RECV_BUFFER_SIZE %d, MAX_LISTEN %d,"
			" MAX_URI_SIZE %d, BUF_SIZE %d\n",
		MAX_RECV_BUFFER_SIZE, MAX_LISTEN, MAX_URI_SIZE, 
		BUF_SIZE );
}

/* debugging function */
/*
void receive_stdin_loop()
{
	#define BSIZE 1024
	char buf[BSIZE+1];
	int len;

	while(1){
		len=fread(buf,1,BSIZE,stdin);
		buf[len+1]=0;
		receive_msg(buf, len);
		printf("-------------------------\n");
	}
}
*/

/* global vars */

int own_pgid = 0; /* whether or not we have our own pgid (and it's ok
					 to use kill(0, sig) */
char* cfg_file = 0;
unsigned int maxbuffer = MAX_RECV_BUFFER_SIZE; /* maximum buffer size we do
												  not want to exceed during the
												  auto-probing procedure; may 
												  be re-configured */
int children_no = 0;			/* number of children processing requests */
#ifdef USE_TCP
int tcp_children_no = 0;
int tcp_disable = 0; /* 1 if tcp is disabled */
#endif
#ifdef USE_TLS
int tls_disable = 0; /* 1 if tls is disabled */
#endif
struct process_table *pt=0;		/*array with children pids, 0= main proc,
									alloc'ed in shared mem if possible*/
int sig_flag = 0;              /* last signal received */
int debug = L_NOTICE;
int dont_fork = 0;
int log_stderr = 0;
/* log facility (see syslog(3)) */
int log_facility = LOG_DAEMON;
int config_check = 0;
/* check if reply first via host==us */
int check_via =  0;        
/* shall use stateful synonym branches? faster but not reboot-safe */
int syn_branch = 1;
/* debugging level for memory stats */
int memlog = L_DBG;
/* should replies include extensive warnings? by default yes,
   good for trouble-shooting
*/
int sip_warning = 1;
/* should localy-generated messages include server's signature?
   be default yes, good for trouble-shooting
*/
int server_signature=1;
/* should ser try to locate outbound interface on multihomed
 * host? by default not -- too expensive
 */
int mhomed=0;
/* use dns and/or rdns or to see if we need to add 
   a ;received=x.x.x.x to via: */
int received_dns = 0;      
char* working_dir = 0;
char* chroot_dir = 0;
char* user=0;
char* group=0;
int uid = 0;
int gid = 0;
char* sock_user=0;
char* sock_group=0;
int sock_uid= -1;
int sock_gid= -1;
int sock_mode= S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP; /* rw-rw---- */

/* more config stuff */
int disable_core_dump=0; /* by default enabled */
int open_files_limit=-1; /* don't touch it by default */
/* a hint to reply modules whether they should send reply
   to IP advertised in Via or IP from which a request came
*/
int reply_to_via=0;

#ifdef USE_MCAST
int mcast_loopback = 0;
int mcast_ttl = -1; /* if -1, don't touch it, use the default (usually 1) */
#endif /* USE_MCAST */

#if 0
char* names[MAX_LISTEN];              /* our names */
int names_len[MAX_LISTEN];            /* lengths of the names*/
struct ip_addr addresses[MAX_LISTEN]; /* our ips */
int addresses_no=0;                   /* number of names/ips */
#endif
struct socket_info* udp_listen=0;
#ifdef USE_TCP
struct socket_info* tcp_listen=0;
#endif
#ifdef USE_TLS
struct socket_info* tls_listen=0;
#endif
struct socket_info* bind_address=0; /* pointer to the crt. proc.
									 listening address*/
struct socket_info* sendipv4; /* ipv4 socket to use when msg. comes from ipv6*/
struct socket_info* sendipv6; /* same as above for ipv6 */
#ifdef USE_TCP
struct socket_info* sendipv4_tcp; 
struct socket_info* sendipv6_tcp; 
#endif
#ifdef USE_TLS
struct socket_info* sendipv4_tls;
struct socket_info* sendipv6_tls;
#endif

unsigned short port_no=0; /* default port*/
#ifdef USE_TLS
unsigned short tls_port_no=0; /* default port */
#endif

struct host_alias* aliases=0; /* name aliases list */

/* ipc related globals */
int process_no = 0;
/* process_bm_t process_bit = 0; */
#ifdef ROUTE_SRV
#endif

/* cfg parsing */
int cfg_errors=0;

/* shared memory (in MB) */
unsigned long shm_mem_size=SHM_MEM_SIZE * 1024 * 1024;

/* export command-line to anywhere else */
int my_argc;
char **my_argv;

#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse();


int is_main=1; /* flag = is this the  "main" process? */

char* pid_file = 0; /* filename as asked by use */
char* pgid_file = 0;


/* call it before exiting; if show_status==1, mem status is displayed */
void cleanup(show_status)
{
	/*clean-up*/
	if (mem_lock) 
		shm_unlock(); /* hack: force-unlock the shared memory lock in case
					 some process crashed and let it locked; this will 
					 allow an almost gracious shutdown */
	destroy_modules();
#ifdef USE_TCP
	destroy_tcp();
#endif
#ifdef USE_TLS
	destroy_tls();
#endif
	destroy_timer();
	close_unixsock_server();
	destroy_fifo();
	destroy_script_cb();
#ifdef PKG_MALLOC
	if (show_status){
		LOG(memlog, "Memory status (pkg):\n");
		pkg_status();
	}
#endif
#ifdef SHM_MEM
	if (pt) shm_free(pt);
	pt=0;
	if (show_status){
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
	}
	/* zero all shmem alloc vars that we still use */
	shm_mem_destroy();
#endif
	if (pid_file) unlink(pid_file);
	if (pgid_file) unlink(pgid_file);
}


/* tries to send a signal to all our processes
 * if daemonized  is ok to send the signal to all the process group,
 * however if not daemonized we might end up sending the signal also
 * to the shell which launched us => most signals will kill it if 
 * it's not in interactive mode and we don't want this. The non-daemonized 
 * case can occur when an error is encountered before daemonize is called 
 * (e.g. when parsing the config file) or when ser is started in "dont-fork"
 *  mode. Sending the signal to all the processes in pt[] will not work
 *  for processes forked from modules (which have no correspondent entry in 
 *  pt), but this can happen only in dont_fork mode (which is only for
 *  debugging). So in the worst case + "dont-fork" we might leave some
 *  zombies. -- andrei */
static void kill_all_children(int signum)
{
	int r;
	if (own_pgid) kill(0, signum);
	else if (pt)
		for (r=1; r<process_count(); r++)
			if (pt[r].pid) kill(pt[r].pid, signum);
}



/* if this handler is called, a critical timeout has occured while
 * waiting for the children to finish => we should kill everything and exit */
static void sig_alarm_kill(int signo)
{
	kill_all_children(SIGKILL); /* this will kill the whole group
								  including "this" process;
								  for debugging replace with SIGABRT
								  (but warning: it might generate lots
								   of cores) */
}


/* like sig_alarm_kill, but the timeout has occured when cleaning up
 * => try to leave a core for future diagnostics */
static void sig_alarm_abort(int signo)
{
	/* LOG is not signal safe, but who cares, we are abort-ing anyway :-) */
	LOG(L_CRIT, "BUG: shutdown timeout triggered, dying...");
	abort();
}



void handle_sigs()
{
	pid_t	chld;
	int	chld_status;

	switch(sig_flag){
		case 0: break; /* do nothing*/
		case SIGPIPE:
				/* SIGPIPE might be rarely received on use of
				   exec module; simply ignore it
				 */
				LOG(L_WARN, "WARNING: SIGPIPE received and ignored\n");
				break;
		case SIGINT:
		case SIGTERM:
			/* we end the program in all these cases */
			if (sig_flag==SIGINT)
				DBG("INT received, program terminates\n");
			else
				DBG("SIGTERM received, program terminates\n");
				
			/* first of all, kill the children also */
			kill_all_children(SIGTERM);

			     /* Wait for all the children to die */
			while(wait(0) > 0);
			
			cleanup(1); /* cleanup & show status*/
			dprint("Thank you for flying " NAME "\n");
			exit(0);
			break;
			
		case SIGUSR1:
#ifdef STATS
			dump_all_statistic();
#endif
#ifdef PKG_MALLOC
			LOG(memlog, "Memory status (pkg):\n");
			pkg_status();
#endif
#ifdef SHM_MEM
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
#endif
			break;
			
		case SIGCHLD:
			while ((chld=waitpid( -1, &chld_status, WNOHANG ))>0) {
				if (WIFEXITED(chld_status)) 
					LOG(L_INFO, "child process %d exited normally,"
							" status=%d\n", chld, 
							WEXITSTATUS(chld_status));
				else if (WIFSIGNALED(chld_status)) {
					LOG(L_INFO, "child process %d exited by a signal"
							" %d\n", chld, WTERMSIG(chld_status));
#ifdef WCOREDUMP
					LOG(L_INFO, "core was %sgenerated\n",
							 WCOREDUMP(chld_status) ?  "" : "not " );
#endif
				}else if (WIFSTOPPED(chld_status)) 
					LOG(L_INFO, "child process %d stopped by a"
								" signal %d\n", chld,
								 WSTOPSIG(chld_status));
			}
#ifndef STOP_JIRIS_CHANGES
			if (dont_fork) {
				LOG(L_INFO, "INFO: dont_fork turned on, living on\n");
				break;
			} 
			LOG(L_INFO, "INFO: terminating due to SIGCHLD\n");
#endif
			/* exit */
			kill_all_children(SIGTERM);
			if (signal(SIGALRM, sig_alarm_kill) == SIG_ERR ) {
				LOG(L_ERR, "ERROR: could not install SIGALARM handler\n");
				/* continue, the process will die anyway if no
				 * alarm is installed which is exactly what we want */
			}
			alarm(60); /* 1 minute close timeout */
			while(wait(0) > 0); /* wait for all the children to terminate*/
			signal(SIGALRM, sig_alarm_abort);
			cleanup(1); /* cleanup & show status*/
			alarm(0);
			signal(SIGALRM, SIG_IGN);
			DBG("terminating due to SIGCHLD\n");
			exit(0);
			break;
		
		case SIGHUP: /* ignoring it*/
					DBG("SIGHUP received, ignoring it\n");
					break;
		default:
			LOG(L_CRIT, "WARNING: unhandled signal %d\n", sig_flag);
	}
	sig_flag=0;
}



/* added by jku; allows for regular exit on a specific signal;
   good for profiling which only works if exited regularly and
   not by default signal handlers
    - modified by andrei: moved most of the stuff to handle_sigs, 
       made it safer for the "fork" case
*/
static void sig_usr(int signo)
{


	if (is_main){
		if (sig_flag==0) sig_flag=signo;
		else /*  previous sig. not processed yet, ignoring? */
			return; ;
		if (dont_fork) 
				/* only one proc, doing everything from the sig handler,
				unsafe, but this is only for debugging mode*/
			handle_sigs();
	}else{
		/* process the important signals */
		switch(signo){
			case SIGPIPE:
					LOG(L_INFO, "INFO: signal %d received\n", signo);
				break;
			case SIGINT:
			case SIGTERM:
					LOG(L_INFO, "INFO: signal %d received\n", signo);
					/* print memory stats for non-main too */
					#ifdef PKG_MALLOC
					LOG(memlog, "Memory status (pkg):\n");
					pkg_status();
					#endif
					exit(0);
					break;
			case SIGUSR1:
				/* statistics, do nothing, printed only from the main proc */
					break;
				/* ignored*/
			case SIGUSR2:
			case SIGHUP:
					break;
			case SIGCHLD:
#ifndef 			STOP_JIRIS_CHANGES
					DBG("SIGCHLD received: "
						"we do not worry about grand-children\n");
#else
					exit(0); /* terminate if one child died */
#endif
		}
	}
}



/* install the signal handlers, returns 0 on success, -1 on error */
int install_sigs()
{
	/* added by jku: add exit handler */
	if (signal(SIGINT, sig_usr) == SIG_ERR ) {
		DPrint("ERROR: no SIGINT signal handler can be installed\n");
		goto error;
	}
	/* if we debug and write to a pipe, we want to exit nicely too */
	if (signal(SIGPIPE, sig_usr) == SIG_ERR ) {
		DPrint("ERROR: no SIGINT signal handler can be installed\n");
		goto error;
	}
	
	if (signal(SIGUSR1, sig_usr)  == SIG_ERR ) {
		DPrint("ERROR: no SIGUSR1 signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGCHLD , sig_usr)  == SIG_ERR ) {
		DPrint("ERROR: no SIGCHLD signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGTERM , sig_usr)  == SIG_ERR ) {
		DPrint("ERROR: no SIGTERM signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGHUP , sig_usr)  == SIG_ERR ) {
		DPrint("ERROR: no SIGHUP signal handler can be installed\n");
		goto error;
	}
	if (signal(SIGUSR2 , sig_usr)  == SIG_ERR ) {
		DPrint("ERROR: no SIGUSR2 signal handler can be installed\n");
		goto error;
	}
	return 0;
error:
	return -1;
}



/* converts a username into uid:gid,
 * returns -1 on error & 0 on success */
static int user2uid(int* uid, int* gid, char* user)
{
	char* tmp;
	struct passwd *pw_entry;
	
	if (user){
		*uid=strtol(user, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			pw_entry=getpwnam(user);
			if (pw_entry==0){
				goto error;
			}
			*uid=pw_entry->pw_uid;
			if (gid) *gid=pw_entry->pw_gid;
		}
		return 0;
	}
error:
	return -1;
}



/* converts a group name into a gid
 * returns -1 on error, 0 on success */
static int group2gid(int* gid, char* group)
{
	char* tmp;
	struct group  *gr_entry;
	
	if (group){
		*gid=strtol(group, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			gr_entry=getgrnam(group);
			if (gr_entry==0){
				goto error;
			}
			*gid=gr_entry->gr_gid;
		}
		return 0;
	}
error:
	return -1;
}



/* main loop */
int main_loop()
{
	int  i;
	pid_t pid;
	struct socket_info* si;
#ifdef USE_TCP
	int sockfd[2];
#endif

	/* one "main" process and n children handling i/o */

	is_main=0;
	if (dont_fork){
#ifdef STATS
		setstats( 0 );
#endif
		if (udp_listen==0){
			LOG(L_ERR, "ERROR: no fork mode requires at least one"
					" udp listen address, exiting...\n");
			goto error;
		}
		/* only one address, we ignore all the others */
		if (udp_init(udp_listen)==-1) goto error;
		bind_address=udp_listen;
		sendipv4=bind_address;
		sendipv6=bind_address; /*FIXME*/
		if (udp_listen->next){
			LOG(L_WARN, "WARNING: using only the first listen address"
						" (no fork)\n");
		}
		/* initialize fifo server -- we need to open the fifo before
		 * do_suid() and start the fifo server after all the socket 
		 * are initialized, to inherit them*/
		if (init_fifo_server()<0) {
			LOG(L_ERR, "initializing fifo server failed\n");
			goto error;
		}
		 /* Initialize Unix domain socket server */
		if (init_unixsock_socket()<0) {
			LOG(L_ERR, "Error while creating unix domain sockets\n");
			goto error;
		}
		if (do_suid()==-1) goto error; /* try to drop privileges */
		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main 
		*/

		/* we need another process to act as the timer*/
#ifdef USE_TCP
		/* if we are using tcp we always need a timer process,
		 * we cannot count on select timeout to measure time
		 * (it works only on linux)
		 */
		if ((!tcp_disable)||(timer_list))
#else
		if (timer_list)
#endif
		{
				process_no++;
				if ((pid=fork())<0){
					LOG(L_CRIT,  "ERROR: main_loop: Cannot fork\n");
					goto error;
				}
				
				if (pid==0){
					/* child */
					/* timer!*/
					/* process_bit = 0; */
					if (init_child(PROC_TIMER) < 0) {
						LOG(L_ERR, "timer: init_child failed\n");
						goto error;
					}
					for(;;){
						sleep(TIMER_TICK);
						timer_ticker();
					}
				}else{
						pt[process_no].pid=pid; /*should be shared mem anyway*/
						strncpy(pt[process_no].desc, "timer", MAX_PT_DESC );
				}
		}

		/* if configured, start a server for accepting FIFO commands,
		 * we need to do it after all the sockets are initialized, to 
		 * inherit them*/
		if (start_fifo_server()<0) {
			LOG(L_ERR, "starting fifo server failed\n");
			goto error;
		}

		if (init_unixsock_children()<0) {
			LOG(L_ERR, "Error while initializing Unix domain socket server\n");
			goto error;
		}

		/* main process, receive loop */
		process_no=0; /*main process number*/
		pt[process_no].pid=getpid();
		snprintf(pt[process_no].desc, MAX_PT_DESC, 
			"stand-alone receiver @ %s:%s", 
			 bind_address->name.s, bind_address->port_no_str.s );
		
		
		     /* We will call child_init even if we
		      * do not fork - and it will be called with rank 1 because
		      * in fact we behave like a child, not like main process
		      */

		if (init_child(1) < 0) {
			LOG(L_ERR, "main_dontfork: init_child failed\n");
			goto error;
		}

		is_main=1; /* hack 42: call init_child with is_main=0 in case
					 some modules wants to fork a child */
		
		return udp_rcv_loop();
	}else{
		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main )
		*/

		for(si=udp_listen;si;si=si->next){
			/* create the listening socket (for each address)*/
			/* udp */
			if (udp_init(si)==-1) goto error;
			/* get first ipv4/ipv6 socket*/
			if ((si->address.af==AF_INET)&&
					((sendipv4==0)||(sendipv4->flags&SI_IS_LO)))
				sendipv4=si;
	#ifdef USE_IPV6
			if((sendipv6==0)&&(si->address.af==AF_INET6))
				sendipv6=si;
	#endif
		}
#ifdef USE_TCP
		if (!tcp_disable){
			for(si=tcp_listen; si; si=si->next){
				/* same thing for tcp */
				if (tcp_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tcp==0)||(sendipv4_tcp->flags&SI_IS_LO)))
					sendipv4_tcp=si;
		#ifdef USE_IPV6
				if((sendipv6_tcp==0)&&(si->address.af==AF_INET6))
					sendipv6_tcp=si;
		#endif
			}
		}
#ifdef USE_TLS
		if (!tls_disable){
			for(si=tls_listen; si; si=si->next){
				/* same as for tcp*/
				if (tls_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tls==0)||(sendipv4_tls->flags&SI_IS_LO)))
					sendipv4_tls=si;
		#ifdef USE_IPV6
				if((sendipv6_tls==0)&&(si->address.af==AF_INET6))
					sendipv6_tls=si;
		#endif
			}
		}
#endif /* USE_TLS */
#endif /* USE_TCP */

		/* initialize fifo server -- we need to open the fifo before
		 * do_suid() and start the fifo server after all the socket 
		 * are initialized, to inherit them*/
		if (init_fifo_server()<0) {
			LOG(L_ERR, "initializing fifo server failed\n");
			goto error;
		}
		 /* Initialize Unix domain socket server */
		     /* Create the unix domain sockets */
		if (init_unixsock_socket()<0) {
			LOG(L_ERR, "ERROR: Could not create unix domain sockets\n");
			goto error;
		}

			/* all processes should have access to all the sockets (for sending)
			 * so we open all first*/
		if (do_suid()==-1) goto error; /* try to drop privileges */

		/* if configured, start a server for accepting FIFO commands,
		 * we need to do it after all the sockets are initialized, to 
		 * inherit them*/
		if (start_fifo_server()<0) {
			LOG(L_ERR, "starting fifo server failed\n");
			goto error;
		}
		     /* Spawn children listening on unix domain socket if and only if
		      * the unix domain socket server has not been disabled (i == 0)
		      */
		if (init_unixsock_children()<0) {
			LOG(L_ERR, "ERROR: Could not initialize unix domain socket server\n");
			goto error;
		}

		/* udp processes */
		for(si=udp_listen; si; si=si->next){
			for(i=0;i<children_no;i++){
				process_no++;
#ifdef USE_TCP
				if(!tcp_disable){
		 			if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
						LOG(L_ERR, "ERROR: main_loop: socketpair failed: %s\n",
							strerror(errno));
						goto error;
					}
				}
#endif
				if ((pid=fork())<0){
					LOG(L_CRIT,  "main_loop: Cannot fork\n");
					goto error;
				}else if (pid==0){
					     /* child */
#ifdef USE_TCP
					if (!tcp_disable){
						close(sockfd[0]);
						unix_tcp_sock=sockfd[1];
					}
#endif
					bind_address=si; /* shortcut */
					if (init_child(i + 1) < 0) {
						LOG(L_ERR, "init_child failed\n");
						goto error;
					}
#ifdef STATS
					setstats( i+r*children_no );
#endif
					return udp_rcv_loop();
				}else{
						pt[process_no].pid=pid; /*should be in shared mem.*/
						snprintf(pt[process_no].desc, MAX_PT_DESC,
							"receiver child=%d sock= %s:%s", i, 	
							si->name.s, si->port_no_str.s );
#ifdef USE_TCP
						if (!tcp_disable){
							close(sockfd[1]);
							pt[process_no].unix_sock=sockfd[0];
							pt[process_no].idx=-1; /* this is not a "tcp"
													  process*/
						}
#endif
				}
			}
			/*parent*/
			/*close(udp_sock)*/; /*if it's closed=>sendto invalid fd errors?*/
		}
	}

	/*this is the main process*/
	bind_address=0;				/* main proc -> it shouldn't send anything, */
	

#ifdef USE_TCP
	/* if we are using tcp we always need the timer */
	if ((!tcp_disable)||(timer_list))
#else
	if (timer_list)
#endif
	{
#ifdef USE_TCP
		if (!tcp_disable){
 			if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd)<0){
				LOG(L_ERR, "ERROR: main_loop: socketpair failed: %s\n",
					strerror(errno));
				goto error;
			}
		}
#endif
		/* fork again for the attendant process*/
		process_no++;
		if ((pid=fork())<0){
			LOG(L_CRIT, "main_loop: cannot fork timer process\n");
			goto error;
		}else if (pid==0){
			/* child */
			/* is_main=0; */
#ifdef USE_TCP
			if (!tcp_disable){
				close(sockfd[0]);
				unix_tcp_sock=sockfd[1];
			}
#endif
			if (init_child(PROC_TIMER) < 0) {
				LOG(L_ERR, "timer: init_child failed\n");
				goto error;
			}
			
			for(;;){
				/* debug:  instead of doing something useful */
				/* (placeholder for timers, etc.) */
				sleep(TIMER_TICK);
				/* if we received a signal => TIMER_TICK may have not elapsed*/
				timer_ticker();
			}
		}else{
			pt[process_no].pid=pid;
			strncpy(pt[process_no].desc, "timer", MAX_PT_DESC );
#ifdef USE_TCP
			if(!tcp_disable){
						close(sockfd[1]);
						pt[process_no].unix_sock=sockfd[0];
						pt[process_no].idx=-1; /* this is not a "tcp" process*/
			}
#endif
		}
	}
#ifdef USE_TCP
		if (!tcp_disable){
				/* start tcp  & tls receivers */
			if (tcp_init_children()<0) goto error;
				/* start tcp+tls master proc */
			process_no++;
			if ((pid=fork())<0){
				LOG(L_CRIT, "main_loop: cannot fork tcp main process\n");
				goto error;
			}else if (pid==0){
				/* child */
				/* is_main=0; */
				if (init_child(PROC_TCP_MAIN) < 0) {
					LOG(L_ERR, "tcp_main: error in init_child\n");
					goto error;
				}
				tcp_main_loop();
			}else{
				pt[process_no].pid=pid;
				strncpy(pt[process_no].desc, "tcp main process", MAX_PT_DESC );
				pt[process_no].unix_sock=-1;
				pt[process_no].idx=-1; /* this is not a "tcp" process*/
				unix_tcp_sock=-1;
			}
		}
#endif
	/* main */
	pt[0].pid=getpid();
	strncpy(pt[0].desc, "attendant", MAX_PT_DESC );
#ifdef USE_TCP
	if(!tcp_disable){
		pt[process_no].unix_sock=-1;
		pt[process_no].idx=-1; /* this is not a "tcp" process*/
		unix_tcp_sock=-1;
	}
#endif
	/*DEBUG- remove it*/
#ifdef DEBUG
	fprintf(stderr, "\n% 3d processes (%3d), % 3d children * "
			"listening addresses + tcp listeners + tls listeners"
			"+ main + fifo %s\n", process_no+1, process_count(), children_no,
			(timer_list)?"+ timer":"");
	for (r=0; r<=process_no; r++){
		fprintf(stderr, "% 3d   % 5d - %s\n", r, pt[r].pid, pt[r].desc);
	}
#endif
	process_no=0; 
	/* process_bit = 0; */
	is_main=1;
	
	if (init_child(PROC_MAIN) < 0) {
		LOG(L_ERR, "main: error in init_child\n");
		goto error;
	}
	for(;;){
			pause();
			handle_sigs();
	}
	
	
	/*return 0; */
 error:
	is_main=1;  /* if we are here, we are the "main process",
				  any forked children should exit with exit(-1) and not
				  ever use return */
	return -1;

}




int main(int argc, char** argv)
{

	FILE* cfg_stream;
	int c,r;
	char *tmp;
	char *options;
	int ret;
	unsigned int seed;
	int rfd;

	/*init*/
	ret=-1;
	my_argc=argc; my_argv=argv;
	
	/*init pkg mallocs (before parsing cfg or cmd line !)*/
	if (init_pkg_mallocs()==-1)
		goto error;

#ifdef DBG_MSG_QA
	fprintf(stderr, "WARNING: ser startup: "
		"DBG_MSG_QA enabled, ser may exit abruptly\n");
#endif



	/* process command line (get port no, cfg. file path etc) */
	opterr=0;
	options=
#ifdef STATS
	"s:"
#endif
	"f:cp:m:b:l:n:N:rRvdDETVhw:t:u:g:P:G:i:x:";
	
	while((c=getopt(argc,argv,options))!=-1){
		switch(c){
			case 'f':
					cfg_file=optarg;
					break;
			case 'c':
					config_check=1;
					log_stderr=1; /* force stderr logging */
					break;
			case 's':
				#ifdef STATS
					stat_file=optarg;
				#endif
					break;
			case 'p':
					port_no=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad port number: -p %s\n", optarg);
						goto error;
					}
					break;
			case 'm':
					shm_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad shmem size number: -m %s\n",
										optarg);
						goto error;
					};
					LOG(L_INFO, "ser: shared memory: %ld bytes\n",
									shm_mem_size );
					break;

			case 'b':
					maxbuffer=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad max buffer size number: -p %s\n",
											optarg);
						goto error;
					}
					break;
			case 'l':
					/* add a new addr. to our address list */
					if (add_listen_iface(optarg, 0, 0, 0)!=0){
						fprintf(stderr, "failed to add new listen address\n");
						goto error;
					}
					break;
			case 'n':
					children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad process number: -n %s\n",
									optarg);
						goto error;
					}
					break;
			case 'v':
					check_via=1;
					break;
			case 'r':
					received_dns|=DO_DNS;
					break;
			case 'R':
					received_dns|=DO_REV_DNS;
			case 'd':
					debug++;
					break;
			case 'D':
					dont_fork=1;
					break;
			case 'E':
					log_stderr=1;
					break;
			case 'T':
#ifdef USE_TCP
					tcp_disable=1;
#else
					fprintf(stderr,"WARNING: tcp support not compiled in\n");
#endif
					break;
			case 'N':
#ifdef USE_TCP
					tcp_children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad process number: -N %s\n",
									optarg);
						goto error;
					}
#else
					fprintf(stderr,"WARNING: tcp support not compiled in\n");
#endif
					break;
			case 'V':
					printf("version: %s\n", version);
					printf("flags: %s\n", flags );
					print_ct_constants();
					printf("%s\n",id);
					printf("%s compiled on %s with %s\n", __FILE__,
							compiled, COMPILER );
					
					exit(0);
					break;
			case 'h':
					printf("version: %s\n", version);
					printf("%s",help_msg);
					exit(0);
					break;
			case 'w':
					working_dir=optarg;
					break;
			case 't':
					chroot_dir=optarg;
					break;
			case 'u':
					user=optarg;
					break;
			case 'g':
					group=optarg;
					break;
			case 'P':
					pid_file=optarg;
					break;
		        case 'G':
				        pgid_file=optarg;
				        break;
			case 'i':
					fifo=optarg;
					break;
			case 'x':
					unixsock_name=optarg;
					break;
			case '?':
					if (isprint(optopt))
						fprintf(stderr, "Unknown option `-%c´.\n", optopt);
					else
						fprintf(stderr, 
								"Unknown option character `\\x%x´.\n",
								optopt);
					goto error;
			case ':':
					fprintf(stderr, 
								"Option `-%c´ requires an argument.\n",
								optopt);
					goto error;
			default:
					abort();
		}
	}
	
	/* fill missing arguments with the default values*/
	if (cfg_file==0) cfg_file=CFG_FILE;

	/* load config file or die */
	cfg_stream=fopen (cfg_file, "r");
	if (cfg_stream==0){
		fprintf(stderr, "ERROR: loading config file(%s): %s\n", cfg_file,
				strerror(errno));
		goto error;
	}

	/* seed the prng */
	/* try to use /dev/urandom if possible */
	seed=0;
	if ((rfd=open("/dev/urandom", O_RDONLY))!=-1){
try_again:
		if (read(rfd, (void*)&seed, sizeof(seed))==-1){
			if (errno==EINTR) goto try_again; /* interrupted by signal */
			LOG(L_WARN, "WARNING: could not read from /dev/urandom (%d)\n",
						errno);
		}
		DBG("read %u from /dev/urandom\n", seed);
			close(rfd);
	}else{
		LOG(L_WARN, "WARNING: could not open /dev/urandom (%d)\n", errno);
	}
	seed+=getpid()+time(0);
	DBG("seeding PRNG with %u\n", seed);
	srand(seed);
	DBG("test random number %u\n", rand());
	
	
	
	/* register a diagnostic FIFO command  - moved to fifo server - bogdan
	if (register_core_fifo()<0) {
		LOG(L_CRIT, "unable to register core FIFO commands\n");
		goto error;
	}*/

	/*register builtin  modules*/
	register_builtin_modules();

	yyin=cfg_stream;
	if ((yyparse()!=0)||(cfg_errors)){
		fprintf(stderr, "ERROR: bad config file (%d errors)\n", cfg_errors);
		goto error;
	}
	
	
	
	print_rl();
	
	/* fix parameters */
	if (port_no<=0) port_no=SIP_PORT;
#ifdef USE_TLS
	if (tls_port_no<=0) tls_port_no=SIPS_PORT;
#endif
	
	
	if (children_no<=0) children_no=CHILD_NO;
#ifdef USE_TCP
	if (!tcp_disable){
		if (tcp_children_no<=0) tcp_children_no=children_no;
	}
#endif
	
	if (working_dir==0) working_dir="/";
	
	/* get uid/gid */
	if (user){
		if (user2uid(&uid, &gid, user)<0){
			fprintf(stderr, "bad user name/uid number: -u %s\n", user);
			goto error;
		}
	}
	if (group){
		if (group2gid(&gid, group)<0){
				fprintf(stderr, "bad group name/gid number: -u %s\n", group);
			goto error;
		}
	}
	/* fix sock/fifo uid/gid */
	if (sock_user){
		if (user2uid(&sock_uid, 0, sock_user)<0){
			fprintf(stderr, "bad socket user name/uid number %s\n", user);
			goto error;
		}
	}
	if (sock_group){
		if (group2gid(&sock_gid, sock_group)<0){
			fprintf(stderr, "bad group name/gid number: -u %s\n", group);
			goto error;
		}
	}
	if (fix_all_socket_lists()!=0){
		fprintf(stderr,  "failed to initialize list addresses\n");
		goto error;
	}
	/* print all the listen addresses */
	printf("Listening on \n");
	print_all_socket_lists();
	printf("Aliases: \n");
	/*print_aliases();*/
	print_aliases();
	printf("\n");
	
	if (dont_fork){
		fprintf(stderr, "WARNING: no fork mode %s\n", 
				(udp_listen)?(
				(udp_listen->next)?" and more than one listen address found"
				"(will use only the the first one)":""
				):"and no udp listen address found" );
	}
	if (config_check){
		fprintf(stderr, "config file ok, exiting...\n");
		goto error;
	}


	/*init shm mallocs
	 *  this must be here 
	 *     -to allow setting shm mem size from the command line
	 *       => if shm_mem should be settable from the cfg file move
	 *       everything after
	 *     -it must be also before init_timer and init_tcp
	 *     -it must be after we know uid (so that in the SYSV sems case,
	 *        the sems will have the correct euid)
	 * --andrei */
	if (init_shm_mallocs()==-1)
		goto error;
	/*init timer, before parsing the cfg!*/
	if (init_timer()<0){
		LOG(L_CRIT, "could not initialize timer, exiting...\n");
		goto error;
	}
	
#ifdef USE_TCP
	if (!tcp_disable){
		/*init tcp*/
		if (init_tcp()<0){
			LOG(L_CRIT, "could not initialize tcp, exiting...\n");
			goto error;
		}
	}
#ifdef USE_TLS
	if (!tls_disable){
		/* init tls*/
		if (init_tls()<0){
			LOG(L_CRIT, "could not initialize tls, exiting...\n");
			goto error;
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */
	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize(argv[0]) <0 ) goto error;
	}
	if (install_sigs() != 0){
		fprintf(stderr, "ERROR: could not install the signal handlers\n");
		goto error;
	}
	
	
	/*alloc pids*/
#ifdef SHM_MEM
	pt=shm_malloc(sizeof(struct process_table)*process_count());
#else
	pt=pkg_malloc(sizeof(struct process_table)*process_count());
#endif
	if (pt==0){
		fprintf(stderr, "ERROR: out  of memory\n");
		goto error;
	}
	memset(pt, 0, sizeof(struct process_table)*process_count());

	if (disable_core_dump) set_core_dump(0, 0);
	else set_core_dump(1, shm_mem_size+PKG_MEM_POOL_SIZE+4*1024*1024);
	if (open_files_limit>0){
		if(increase_open_fds(open_files_limit)<0){ 
			fprintf(stderr, "ERROR: error could not increase file limits\n");
			goto error;
		}
	}
	
	if (init_modules() != 0) {
		fprintf(stderr, "ERROR: error while initializing modules\n");
		goto error;
	}
	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		fprintf(stderr, "ERROR: error %d while trying to fix configuration\n",
						r);
		goto error;
	};

#ifdef STATS
	if (init_stats(  dont_fork ? 1 : children_no  )==-1) goto error;
#endif
	
	ret=main_loop();
	/*kill everything*/
	kill_all_children(SIGTERM);
	/*clean-up*/
	cleanup(0);
	return ret;

error:
	/*kill everything*/
	kill_all_children(SIGTERM);
	/*clean-up*/
	cleanup(0);
	return -1;

}

