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
 *
 * History:
 * -------
 * 2002-01-29 argc/argv globalized via my_{argc|argv} (jiri)
 * 2001-01-23 mhomed added (jiri)
 *
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
#include "name_alias.h"
#include "hash_func.h"
#include "pt.h"
#ifdef USE_TCP
#include "tcp_init.h"
#endif


#include "stats.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

static char id[]="@(#) $Id$";
static char version[]=  NAME " " VERSION " (" ARCH "/" OS ")" ;
static char compiled[]= __TIME__ " " __DATE__ ;
static char flags[]=
"STATS:"
#ifdef STATS
"On"
#else
"Off"
#endif
#ifdef USE_IPV6
", USE_IPV6"
#endif
#ifdef USE_TCP
", USE_TCP"
#endif
#ifdef NO_DEBUG
", NO_DEBUG"
#endif
#ifdef NO_LOG
", NO_LOG"
#endif
#ifdef EXTRA_DEBUG
", EXTRA_DEBUG"
#endif
#ifdef DNS_IP_HACK
", DNS_IP_HACK"
#endif
#ifdef SHM_MEM
", SHM_MEM"
#endif
#ifdef SHM_MMAP
", SHM_MMAP"
#endif
#ifdef PKG_MALLOC
", PKG_MALLOC"
#endif
#ifdef VQ_MALLOC
", VQ_MALLOC"
#endif
#ifdef F_MALLOC
", F_MALLOC"
#endif
#ifdef USE_SHM_MEM
", USE_SHM_MEM"
#endif
#ifdef DBG_QM_MALLOC
", DBG_QM_MALLOC"
#endif
#ifdef DEBUG_DMALLOC
", DEBUG_DMALLOC"
#endif
#ifdef FAST_LOCK
", FAST_LOCK"
#ifdef BUSY_WAIT
"-BUSY_WAIT"
#endif
#ifdef USE_PTHREAD_MUTEX
", USE_PTHREAD_MUTEX"
#endif
#ifdef USE_POSIX_SEM
", USE_POSIX_SEM"
#endif
#ifdef USE_SYSV_SEM
", USE_SYSV_SEM"
#endif
#ifdef ADAPTIVE_WAIT
"-ADAPTIVE_WAIT"
#endif
#ifdef NOSMP
"-NOSMP"
#endif
#endif /*FAST_LOCK*/
;

static char help_msg[]= "\
Usage: " NAME " -l address [-p port] [-l address [-p port]...] [options]\n\
Options:\n\
    -f file      Configuration file (default " CFG_FILE ")\n\
    -p port      Listen on the specified port (default: 5060)\n\
                 applies to the last address in -l and to all \n\
                 following that do not have a corespponding -p\n\
    -l address   Listen on the specified address (multiple -l mean\n\
                 listening on more addresses). The default behaviour\n\
                 is to listen on the addresses returned by uname(2)\n\
\n\
    -n processes Number of child processes to fork per interface\n\
                 (default: 8)\n\
\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                 field to a via\n\
    -R           Same as `-r` but use reverse dns;\n\
                 (to use both use `-rR`)\n\
\n\
    -v           Turn on \"via:\" host checking when forwarding replies\n\
    -d           Debugging mode (multiple -d increase the level)\n\
    -D           Do not fork into daemon mode\n\
    -E           Log to stderr\n\
    -V           Version number\n\
    -h           This help message\n\
    -b nr        Maximum receive buffer size which will not be exceeded by\n\
                 auto-probing procedure even if  OS allows\n\
    -m nr        Size of shared memory allocated in Megabytes\n\
    -w  dir      change the working directory to \"dir\" (default \"/\")\n\
    -t  dir      chroot to \"dir\"\n\
    -u uid       change uid \n\
    -g gid       change gid \n\
    -P file      create a pid file\n\
    -i fifo_path create a fifo (usefull for monitoring " NAME ") \n"
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

/* debuging function */
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

char* cfg_file = 0;
unsigned int maxbuffer = MAX_RECV_BUFFER_SIZE; /* maximum buffer size we do
												  not want to exceed durig the
												  auto-probing procedure; may 
												  be re-configured */
int children_no = 0;			/* number of children processing requests */
#ifdef USE_TCP
int tcp_children_no = 0;
#endif
struct process_table *pt=0;		/*array with childrens pids, 0= main proc,
									alloc'ed in shared mem if possible*/
int sig_flag = 0;              /* last signal received */
int debug = 0;
int dont_fork = 0;
int log_stderr = 0;
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
/* a hint to reply modules whether they should send reply
   to IP advertised in Via or IP from which a request came
*/
int reply_to_via=0;

#if 0
char* names[MAX_LISTEN];              /* our names */
int names_len[MAX_LISTEN];            /* lengths of the names*/
struct ip_addr addresses[MAX_LISTEN]; /* our ips */
int addresses_no=0;                   /* number of names/ips */
#endif
struct socket_info sock_info[MAX_LISTEN];/*all addresses we listen/send from*/
#ifdef USE_TCP
struct socket_info tcp_info[MAX_LISTEN];/*all tcp addresses we listen on*/
#endif
int sock_no=0; /* number of addresses/open sockets*/
struct socket_info* bind_address=0; /* pointer to the crt. proc.
									 listening address*/
int bind_idx; /* same as above but index in the bound[] array */
struct socket_info* sendipv4; /* ipv4 socket to use when msg. comes from ipv6*/
struct socket_info* sendipv6; /* same as above for ipv6 */
#ifdef USE_TCP
struct socket_info* sendipv4_tcp; 
struct socket_info* sendipv6_tcp; 
#endif

unsigned short port_no=0; /* default port*/

struct host_alias* aliases=0; /* name aliases list */

/* ipc related globals */
int process_no = 0;
/* process_bm_t process_bit = 0; */
#ifdef ROUTE_SRV
#endif

/* cfg parsing */
int cfg_errors=0;

/* shared memory (in MB) */
unsigned int shm_mem_size=SHM_MEM_SIZE * 1024 * 1024;

/* export command-line to anywhere else */
int my_argc;
char **my_argv;

#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse();


int is_main=0; /* flag = is this the  "main" process? */

char* pid_file = 0; /* filename as asked by use */



/* callit before exiting; if show_status==1, mem status is displayed */
void cleanup(show_status)
{
	/*clean-up*/
	destroy_modules();
#ifdef USE_TCP
	destroy_tcp();
#endif
	destroy_timer();
#ifdef PKG_MALLOC
	if (show_status){
		LOG(memlog, "Memory status (pkg):\n");
		pkg_status();
	}
#endif
#ifdef SHM_MEM
	shm_free(pt);
	pt=0;
	if (show_status){
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
	}
	/* zero all shmem alloc vars that we still use */
	shm_mem_destroy();
#endif
	if (pid_file) unlink(pid_file);
}



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

	if (gid&&(setgid(gid)<0)){
		LOG(L_CRIT, "cannot change gid to %d: %s\n", gid, strerror(errno));
		goto error;
	}
	
	if(uid&&(setuid(uid)<0)){
		LOG(L_CRIT, "cannot change uid to %d: %s\n", uid, strerror(errno));
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
	for (r=3;r<MAX_FD; r++){
			close(r);
	}
	
	if (log_stderr==0)
		openlog(name, LOG_PID|LOG_CONS, LOG_DAEMON);
		/* LOG_CONS, LOG_PERRROR ? */
	return  0;

error:
	return -1;
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
#ifdef OBSOLETED
			else if (sig_flag==SIGPIPE) 
				DBG("SIGPIPE received, program terminates\n");
#endif
			else
				DBG("SIGTERM received, program terminates\n");
				
			/* first of all, kill the children also */
			kill(0, SIGTERM);

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
			kill(0, SIGTERM);
			while(wait(0) > 0); /* wait for all the children to terminate*/
			cleanup(1); /* cleanup & show status*/
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



/* main loop */
int main_loop()
{
	int r, i;
	pid_t pid;
#ifdef USE_TCP
	int sockfd[2];
#endif
#ifdef WITH_SNMP_MOD
	int (*snmp_start)();

	/* initialize snmp module */
	snmp_start = (int(*)())find_export("snmp_start", 0);
	if(snmp_start)
		if(snmp_start() == -1)
			LOG(L_ERR, "ERROR: Couldn't start snmp agent\n");
#endif
		

	/* one "main" process and n children handling i/o */


	if (dont_fork){
#ifdef STATS
		setstats( 0 );
#endif
		/* only one address, we ignore all the others */
		if (udp_init(&sock_info[0])==-1) goto error;
		bind_address=&sock_info[0];
		bind_idx=0;
		if (sock_no>1){
			LOG(L_WARN, "WARNING: using only the first listen address"
						" (no fork)\n");
		}

		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main 
		*/

		/* we need another process to act as the timer*/
#ifndef USE_TCP
		/* if we are using tcp we always need a timer process,
		 * we cannot count on select timeout to measure time
		 * (it works only on linux)
		 */
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
					for(;;){
						sleep(TIMER_TICK);
						timer_ticker();
					}
				}else{
						pt[process_no].pid=pid; /*should be shared mem anway*/
						strncpy(pt[process_no].desc, "timer", MAX_PT_DESC );
				}
		}

		/* if configured to do so, start a server for accepting FIFO commands */
		if (open_fifo_server()<0) {
			LOG(L_ERR, "opening fifo server failed\n");
			goto error;
		}
		/* main process, receive loop */
		process_no=0; /*main process number*/
		pt[process_no].pid=getpid();
		snprintf(pt[process_no].desc, MAX_PT_DESC, 
			"stand-alone receiver @ %s:%s", 
			 bind_address->name.s, bind_address->port_no_str.s );
		
		
		     /* We will call child_init even if we
		      * do not fork
		      */

		if (init_child(0) < 0) {
			LOG(L_ERR, "init_child failed\n");
			goto error;
		}

		is_main=1; /* hack 42: call init_child with is_main=0 in case
					 some modules wants to fork a child */
		
		return udp_rcv_loop();
	}else{
		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main )
		*/
		for(r=0;r<sock_no;r++){
			/* create the listening socket (for each address)*/
			/* udp */
			if (udp_init(&sock_info[r])==-1) goto error;
			/* get first ipv4/ipv6 socket*/
			if ((sock_info[r].address.af==AF_INET)&&
					((sendipv4==0)||(sendipv4->is_lo)))
				sendipv4=&sock_info[r];
	#ifdef USE_IPV6
			if((sendipv6==0)&&(sock_info[r].address.af==AF_INET6))
				sendipv6=&sock_info[r];
	#endif
#ifdef USE_TCP
			tcp_info[r]=sock_info[r]; /* copy the sockets */
			/* same thing for tcp */
			if (tcp_init(&tcp_info[r])==-1)  goto error;
			/* get first ipv4/ipv6 socket*/
			if ((tcp_info[r].address.af==AF_INET)&&
					((sendipv4_tcp==0)||(sendipv4_tcp->is_lo)))
				sendipv4_tcp=&tcp_info[r];
	#ifdef USE_IPV6
			if((sendipv6_tcp==0)&&(tcp_info[r].address.af==AF_INET6))
				sendipv6_tcp=&tcp_info[r];
	#endif
#endif
			/* all procs should have access to all the sockets (for sending)
			 * so we open all first*/
		}
		for(r=0; r<sock_no;r++){
			for(i=0;i<children_no;i++){
				process_no++;
#ifdef USE_TCP
		 		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)<0){
					LOG(L_ERR, "ERROR: main_loop: socketpair failed: %s\n",
						strerror(errno));
					goto error;
				}
#endif
				if ((pid=fork())<0){
					LOG(L_CRIT,  "main_loop: Cannot fork\n");
					goto error;
				}else if (pid==0){
					     /* child */
#ifdef USE_TCP
					close(sockfd[0]);
					unix_tcp_sock=sockfd[1];
#endif
					bind_address=&sock_info[r]; /* shortcut */
					bind_idx=r;
					if (init_child(i) < 0) {
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
							"receiver child=%d sock=%d @ %s:%s", i, r, 	
							sock_info[r].name.s, sock_info[r].port_no_str.s );
#ifdef USE_TCP
						close(sockfd[1]);
						pt[process_no].unix_sock=sockfd[0];
						pt[process_no].idx=-1; /* this is not "tcp" process*/
#endif
				}
			}
			/*parent*/
			/*close(udp_sock)*/; /*if it's closed=>sendto invalid fd errors?*/
		}
	}

	/*this is the main process*/
	bind_address=&sock_info[0]; /* main proc -> it shoudln't send anything, */
	bind_idx=0;					/* if it does it will use the first address */
	
	/* if configured to do so, start a server for accepting FIFO commands */
	if (open_fifo_server()<0) {
		LOG(L_ERR, "opening fifo server failed\n");
		goto error;
	}

#ifndef USE_TCP
	/* if we are using tcp we always need the timer */
	if (timer_list)
#endif
	{
#ifdef USE_TCP
 		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd)<0){
			LOG(L_ERR, "ERROR: main_loop: socketpair failed: %s\n",
				strerror(errno));
			goto error;
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
			close(sockfd[0]);
			unix_tcp_sock=sockfd[1];
#endif
			for(;;){
				/* debug:  instead of doing something usefull */
				/* (placeholder for timers, etc.) */
				sleep(TIMER_TICK);
				/* if we received a signal => TIMER_TICK may have not elapsed*/
				timer_ticker();
			}
		}else{
			pt[process_no].pid=pid;
			strncpy(pt[process_no].desc, "timer", MAX_PT_DESC );
#ifdef USE_TCP
						close(sockfd[1]);
						pt[process_no].unix_sock=sockfd[0];
						pt[process_no].idx=-1; /* this is not a "tcp" process*/
#endif
		}
	}
#ifdef USE_TCP
			/* start tcp receivers */
		if (tcp_init_children()<0) goto error;
			/* start tcp master proc */
		process_no++;
		if ((pid=fork())<0){
			LOG(L_CRIT, "main_loop: cannot fork tcp main process\n");
			goto error;
		}else if (pid==0){
			/* child */
			/* is_main=0; */
			tcp_main_loop();
		}else{
			pt[process_no].pid=pid;
			strncpy(pt[process_no].desc, "tcp main process", MAX_PT_DESC );
			pt[process_no].unix_sock=-1;
			pt[process_no].idx=-1; /* this is not a "tcp" process*/
			unix_tcp_sock=-1;
		}
#endif
	/* main */
	pt[0].pid=getpid();
	strncpy(pt[0].desc, "attendant", MAX_PT_DESC );
#ifdef USE_TCP
	pt[process_no].unix_sock=-1;
	pt[process_no].idx=-1; /* this is not a "tcp" process*/
	unix_tcp_sock=-1;
#endif
	/*DEBUG- remove it*/
#ifdef DEBUG
	printf("\n% 3d processes, % 3d children * % 3d listening addresses + main"
			" + fifo %s\n", process_no+1, children_no, sock_no,
			(timer_list)?"+ timer":"");
	for (r=0; r<=process_no; r++){
		printf("% 3d   % 5d\n", r, pt[r].pid);
	}
#endif
	process_no=0; 
	/* process_bit = 0; */
	is_main=1;
	
	for(;;){
			pause();
			handle_sigs();
	}
	
	
	/*return 0; */
 error:
	return -1;

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
			case SIGINT:
			case SIGPIPE:
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
					LOG(L_INFO, "INFO: SIGCHLD received: "
						"we do not worry about grand-children\n");
#else
					exit(0); /* terminate if one child died */
#endif
		}
	}
}



/* add all family type addresses of interface if_name to the socket_info array
 * if if_name==0, adds all addresses on all interfaces
 * WARNING: it only works with ipv6 addresses on FreeBSD
 * return: -1 on error, 0 on success
 */
int add_interfaces(char* if_name, int family, unsigned short port)
{
	struct ifconf ifc;
	struct ifreq ifr;
	struct ifreq ifrcopy;
	char*  last;
	char* p;
	int size;
	int lastlen;
	int s;
	char* tmp;
	struct ip_addr addr;
	int ret;

#ifdef HAVE_SOCKADDR_SA_LEN
	#ifndef MAX
		#define MAX(a,b) ( ((a)>(b))?(a):(b))
	#endif
#endif
	/* ipv4 or ipv6 only*/
	s=socket(family, SOCK_DGRAM, 0);
	ret=-1;
	lastlen=0;
	ifc.ifc_req=0;
	for (size=10; ; size*=2){
		ifc.ifc_len=size*sizeof(struct ifreq);
		ifc.ifc_req=(struct ifreq*) malloc(size*sizeof(struct ifreq));
		if (ifc.ifc_req==0){
			fprintf(stderr, "memory allocation failure\n");
			goto error;
		}
		if (ioctl(s, SIOCGIFCONF, &ifc)==-1){
			if(errno==EBADF) return 0; /* invalid descriptor => no such ifs*/
			fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
			goto error;
		}
		if  ((lastlen) && (ifc.ifc_len==lastlen)) break; /*success,
														   len not changed*/
		lastlen=ifc.ifc_len;
		/* try a bigger array*/
		free(ifc.ifc_req);
	}
	
	last=(char*)ifc.ifc_req+ifc.ifc_len;
	for(p=(char*)ifc.ifc_req; p<last;
			p+=(sizeof(ifr.ifr_name)+
			#ifdef  HAVE_SOCKADDR_SA_LEN
				MAX(ifr.ifr_addr.sa_len, sizeof(struct sockaddr))
			#else
				( (ifr.ifr_addr.sa_family==AF_INET)?
					sizeof(struct sockaddr_in):
					((ifr.ifr_addr.sa_family==AF_INET6)?
						sizeof(struct sockaddr_in6):sizeof(struct sockaddr)) )
			#endif
				)
		)
	{
		/* copy contents into ifr structure
		 * warning: it might be longer (e.g. ipv6 address) */
		memcpy(&ifr, p, sizeof(ifr));
		if (ifr.ifr_addr.sa_family!=family){
			/*printf("strange family %d skipping...\n",
					ifr->ifr_addr.sa_family);*/
			continue;
		}
		
		/*get flags*/
		ifrcopy=ifr;
		if (ioctl(s, SIOCGIFFLAGS,  &ifrcopy)!=-1){ /* ignore errors */
			/* ignore down ifs only if listening on all of them*/
			if (if_name==0){ 
				/* if if not up, skip it*/
				if (!(ifrcopy.ifr_flags & IFF_UP)) continue;
			}
		}
		
		
		
		if ((if_name==0)||
			(strncmp(if_name, ifr.ifr_name, sizeof(ifr.ifr_name))==0)){
			
				/*add address*/
			if (sock_no<MAX_LISTEN){
				sockaddr2ip_addr(&addr, 
					(struct sockaddr*)(p+(long)&((struct ifreq*)0)->ifr_addr));
				if ((tmp=ip_addr2a(&addr))==0) goto error;
				/* fill the strings*/
				sock_info[sock_no].name.s=(char*)malloc(strlen(tmp)+1);
				if(sock_info[sock_no].name.s==0){
					fprintf(stderr, "Out of memory.\n");
					goto error;
				}
				/* fill in the new name and port */
				sock_info[sock_no].name.len=strlen(tmp);
				strncpy(sock_info[sock_no].name.s, tmp, 
							sock_info[sock_no].name.len+1);
				sock_info[sock_no].port_no=port;
				/* mark if loopback */
				if (ifrcopy.ifr_flags & IFF_LOOPBACK) 
					sock_info[sock_no].is_lo=1;
				sock_no++;
				ret=0;
			}else{
				fprintf(stderr, "Too many addresses (max %d)\n", MAX_LISTEN);
				goto error;
			}
		}
			/*
			printf("%s:\n", ifr->ifr_name);
			printf("        ");
			print_sockaddr(&(ifr->ifr_addr));
			printf("        ");
			ls_ifflags(ifr->ifr_name, family, options);
			printf("\n");*/
	}
	free(ifc.ifc_req); /*clean up*/
	close(s);
	return  ret;
error:
	if (ifc.ifc_req) free(ifc.ifc_req);
	close(s);
	return -1;
}



int main(int argc, char** argv)
{

	FILE* cfg_stream;
	struct hostent* he;
	int c,r,t;
	char *tmp;
	char** h;
	struct host_alias* a;
	struct utsname myname;
	char *options;
	char port_no_str[MAX_PORT_LEN];
	int port_no_str_len;
	int ret;
	struct passwd *pw_entry;
	struct group  *gr_entry;
	unsigned int seed;
	int rfd;

	/*init*/
	port_no_str_len=0;
	ret=-1;
	my_argc=argc; my_argv=argv;
	
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
	"f:p:m:b:l:n:rRvdDEVhw:t:u:g:P:i:";
	
	while((c=getopt(argc,argv,options))!=-1){
		switch(c){
			case 'f':
					cfg_file=optarg;
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
					if (sock_no>0) sock_info[sock_no-1].port_no=port_no;
					break;

			case 'm':
					shm_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad shmem size number: -m %s\n",
										optarg);
						goto error;
					};
					LOG(L_INFO, "ser: shared memory allocated: %d MByte\n",
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
					if (sock_no < MAX_LISTEN){
						sock_info[sock_no].name.s=
										(char*)malloc(strlen(optarg)+1);
						if (sock_info[sock_no].name.s==0){
							fprintf(stderr, "Out of memory.\n");
							goto error;
						}
						strncpy(sock_info[sock_no].name.s, optarg,
												strlen(optarg)+1);
						sock_info[sock_no].name.len=strlen(optarg);
						/* set default port */
						sock_info[sock_no].port_no=port_no;
						sock_no++;
					}else{
						fprintf(stderr, 
									"Too many addresses (max. %d).\n",
									MAX_LISTEN);
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
			case 'i':
					fifo=optarg;
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
	/* try to use /dev/random if possible */
	seed=0;
	if ((rfd=open("/dev/random", O_RDONLY))!=-1){
try_again:
		if (read(rfd, (void*)&seed, sizeof(seed))==-1){
			if (errno==EINTR) goto try_again; /* interrupted by signal */
			LOG(L_WARN, "WARNING: could not read from /dev/random (%d)\n",
						errno);
		}
		DBG("read %u from /dev/random\n", seed);
			close(rfd);
	}else{
		LOG(L_WARN, "WARNING: could not open /dev/random (%d)\n", errno);
	}
	seed+=getpid()+time(0);
	DBG("seeding PRNG with %u\n", seed);
	srand(seed);
	DBG("test random number %u\n", rand());
	
	
	/* init hash fucntion */
	if (init_hash()<0) {
		LOG(L_ERR, "ERROR: init_hash failed\n");
		goto error;
	}

	/*init mallocs (before parsing cfg !)*/
	if (init_mallocs()==-1)
		goto error;

	/*init timer, before parsing the cfg!*/
	if (init_timer()<0){
		LOG(L_CRIT, "could not initialize timer, exiting...\n");
		goto error;
	}
#ifdef USE_TCP
	/*init tcp*/
	if (init_tcp()<0){
		LOG(L_CRIT, "could not initialize tcp, exiting...\n");
		goto error;
	}
#endif
	
	/* register a diagnostic FIFO command */
	if (register_core_fifo()<0) {
		LOG(L_CRIT, "unable to register core FIFO commands\n");
		goto error;
	}

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

	
	if (children_no<=0) children_no=CHILD_NO;
#ifdef USE_TCP
	tcp_children_no=children_no;
#endif
#ifdef _OBSOLETED
	else if (children_no >= MAX_PROCESSES ) {
		fprintf(stderr, "ERROR: too many children processes configured;"
				" maximum is %d\n",
			MAX_PROCESSES-1 );
		goto error;
	}
#endif
	
	if (working_dir==0) working_dir="/";
	
	/* get uid/gid */
	if (user){
		uid=strtol(user, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			pw_entry=getpwnam(user);
			if (pw_entry==0){
				fprintf(stderr, "bad user name/uid number: -u %s\n", user);
				goto error;
			}
			uid=pw_entry->pw_uid;
			gid=pw_entry->pw_gid;
		}
	}
	if (group){
		gid=strtol(user, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			gr_entry=getgrnam(group);
			if (gr_entry==0){
				fprintf(stderr, "bad group name/gid number: -u %s\n", group);
				goto error;
			}
			gid=gr_entry->gr_gid;
		}
	}

	if (sock_no==0) {
		/* try to get all listening ipv4 interfaces */
		if (add_interfaces(0, AF_INET, 0)==-1){
			/* if error fall back to get hostname*/
			/* get our address, only the first one */
			if (uname (&myname) <0){
				fprintf(stderr, "cannot determine hostname, try -l address\n");
				goto error;
			}
			sock_info[sock_no].name.s=(char*)malloc(strlen(myname.nodename)+1);
			if (sock_info[sock_no].name.s==0){
				fprintf(stderr, "Out of memory.\n");
				goto error;
			}
			sock_info[sock_no].name.len=strlen(myname.nodename);
			strncpy(sock_info[sock_no].name.s, myname.nodename,
					sock_info[sock_no].name.len+1);
			sock_no++;
		}
	}

	/* try to change all the interface names into addresses
	 *  --ugly hack */
	for (r=0; r<sock_no;){
		if (add_interfaces(sock_info[r].name.s, AF_INET,
					sock_info[r].port_no)!=-1){
			/* success => remove current entry (shift the entire array)*/
			free(sock_info[r].name.s);
			memmove(&sock_info[r], &sock_info[r+1], 
						(sock_no-r)*sizeof(struct socket_info));
			sock_no--;
			continue;
		}
		r++;
	}
	/* get ips & fill the port numbers*/
#ifdef EXTRA_DEBUG
	printf("Listening on \n");
#endif
	for (r=0; r<sock_no;r++){
		/* fix port number, port_no should be !=0 here */
		if (sock_info[r].port_no==0) sock_info[r].port_no=port_no;
		port_no_str_len=snprintf(port_no_str, MAX_PORT_LEN, ":%d", 
									(unsigned short) sock_info[r].port_no);
		/* if buffer too small, snprintf may return per C99 estimated size
		   of needed space; there is no guarantee how many characters 
		   have been written to the buffer and we can be happy if
		   the snprintf implementation zero-terminates whatever it wrote
		   -jku
		*/
		if (port_no_str_len<0 || port_no_str_len>=MAX_PORT_LEN){
			fprintf(stderr, "ERROR: bad port number: %d\n", 
						sock_info[r].port_no);
			goto error;
		}
		/* on some systems snprintf returns really strange things if it does 
		  not have  enough space */
		port_no_str_len=
				(port_no_str_len<MAX_PORT_LEN)?port_no_str_len:MAX_PORT_LEN;
		sock_info[r].port_no_str.s=(char*)malloc(strlen(port_no_str)+1);
		if (sock_info[r].port_no_str.s==0){
			fprintf(stderr, "Out of memory.\n");
			goto error;
		}
		strncpy(sock_info[r].port_no_str.s, port_no_str,
					strlen(port_no_str)+1);
		sock_info[r].port_no_str.len=strlen(port_no_str);
		
		/* get "official hostnames", all the aliases etc. */
		he=resolvehost(sock_info[r].name.s);
		if (he==0){
			DPrint("ERROR: could not resolve %s\n", sock_info[r].name.s);
			goto error;
		}
		/* check if we got the official name */
		if (strcasecmp(he->h_name, sock_info[r].name.s)!=0){
			if (add_alias(sock_info[r].name.s, sock_info[r].name.len,
							sock_info[r].port_no)<0){
				LOG(L_ERR, "ERROR: main: add_alias failed\n");
			}
			/* change the oficial name */
			free(sock_info[r].name.s);
			sock_info[r].name.s=(char*)malloc(strlen(he->h_name)+1);
			if (sock_info[r].name.s==0){
				fprintf(stderr, "Out of memory.\n");
				goto error;
			}
			sock_info[r].name.len=strlen(he->h_name);
			strncpy(sock_info[r].name.s, he->h_name, sock_info[r].name.len+1);
		}
		/* add the aliases*/
		for(h=he->h_aliases; h && *h; h++)
			if (add_alias(*h, strlen(*h), sock_info[r].port_no)<0){
				LOG(L_ERR, "ERROR: main: add_alias failed\n");
			}
		hostent2ip_addr(&sock_info[r].address, he, 0); /*convert to ip_addr 
														 format*/
		if ((tmp=ip_addr2a(&sock_info[r].address))==0) goto error;
		sock_info[r].address_str.s=(char*)malloc(strlen(tmp)+1);
		if (sock_info[r].address_str.s==0){
			fprintf(stderr, "Out of memory.\n");
			goto error;
		}
		strncpy(sock_info[r].address_str.s, tmp, strlen(tmp)+1);
		/* set is_ip (1 if name is an ip address, 0 otherwise) */
		sock_info[r].address_str.len=strlen(tmp);
		if 	(	(sock_info[r].address_str.len==sock_info[r].name.len)&&
				(strncasecmp(sock_info[r].address_str.s, sock_info[r].name.s,
						 sock_info[r].address_str.len)==0)
			){
				sock_info[r].is_ip=1;
				/* do rev. dns on it (for aliases)*/
				he=rev_resolvehost(&sock_info[r].address);
				if (he==0){
					DPrint("WARNING: could not rev. resolve %s\n",
							sock_info[r].name.s);
				}else{
					/* add the aliases*/
					if (add_alias(he->h_name, strlen(he->h_name),
									sock_info[r].port_no)<0){
						LOG(L_ERR, "ERROR: main: add_alias failed\n");
					}
					for(h=he->h_aliases; h && *h; h++)
						if (add_alias(*h,strlen(*h),sock_info[r].port_no)<0){
							LOG(L_ERR, "ERROR: main: add_alias failed\n");
						}
				}
		}else{ sock_info[r].is_ip=0; };
			
#ifdef EXTRA_DEBUG
		printf("              %.*s [%s]:%s\n", sock_info[r].name.len, 
				sock_info[r].name.s,
				sock_info[r].address_str.s, sock_info[r].port_no_str.s);
#endif
	}
	/* removing duplicate addresses*/
	for (r=0; r<sock_no; r++){
		for (t=r+1; t<sock_no;){
			if ((sock_info[r].port_no==sock_info[t].port_no) &&
				(sock_info[r].address.af==sock_info[t].address.af) &&
				(memcmp(sock_info[r].address.u.addr, 
						sock_info[t].address.u.addr,
						sock_info[r].address.len)  == 0)
				){
#ifdef EXTRA_DEBUG
				printf("removing duplicate (%d) %s [%s] == (%d) %s [%s]\n",
						r, sock_info[r].name.s, sock_info[r].address_str.s,
						t, sock_info[t].name.s, sock_info[t].address_str.s);
#endif
				/* add the name to the alias list*/
				if ((!sock_info[t].is_ip) && (
						(sock_info[t].name.len!=sock_info[r].name.len)||
						(strncmp(sock_info[t].name.s, sock_info[r].name.s,
								 sock_info[r].name.len)!=0))
					)
					add_alias(sock_info[t].name.s, sock_info[t].name.len,
								sock_info[t].port_no);
						
				/* free space*/
				free(sock_info[t].name.s);
				free(sock_info[t].address_str.s);
				free(sock_info[t].port_no_str.s);
				/* shift the array*/
				memmove(&sock_info[t], &sock_info[t+1], 
							(sock_no-t)*sizeof(struct socket_info));
				sock_no--;
				continue;
			}
			t++;
		}
	}
	/* print all the listen addresses */
	printf("Listening on \n");
	for (r=0; r<sock_no; r++)
		printf("              %s [%s]:%s\n",sock_info[r].name.s,
				sock_info[r].address_str.s, sock_info[r].port_no_str.s);

	printf("Aliases: ");
	for(a=aliases; a; a=a->next) 
		if (a->port)
			printf("%.*s:%d ", a->alias.len, a->alias.s, a->port);
		else
			printf("%.*s:* ", a->alias.len, a->alias.s);
	printf("\n");
	if (sock_no==0){
		fprintf(stderr, "ERROR: no listening sockets");
		goto error;
	}
	if (dont_fork){
		fprintf(stderr, "WARNING: no fork mode %s\n", 
				(sock_no>1)?" and more than one listen address found (will"
							" use only the the first one)":"");
	}
	
	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize(argv[0]) <0 ) goto error;
	}
	if (init_modules() != 0) {
		fprintf(stderr, "ERROR: error while initializing modules\n");
		goto error;
	}
	
	/*alloc pids*/
#ifdef SHM_MEM
	pt=shm_malloc(sizeof(struct process_table)*process_count());
#else
	pt=malloc(sizeof(struct process_table)*process_count());
#endif
	if (pt==0){
		fprintf(stderr, "ERROR: out  of memory\n");
		goto error;
	}
	memset(pt, 0, sizeof(struct process_table)*process_count());
	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		fprintf(stderr, "ERROR: error %x while trying to fix configuration\n",
						r);
		goto error;
	};

#ifdef STATS
	if (init_stats(  dont_fork ? 1 : children_no  )==-1) goto error;
#endif
	
	ret=main_loop();
	/*kill everything*/
	kill(0, SIGTERM);
	/*clean-up*/
	cleanup(0);
	return ret;

error:
	/*kill everything*/
	kill(0, SIGTERM);
	/*clean-up*/
	cleanup(0);
	return -1;

}

