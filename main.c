/*
 * $Id$
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
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <net/if.h>
#ifdef __sun__
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
#include "hash_func.h"


#include "stats.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

static char id[]="@(#) $Id$";
static char version[]=  NAME " " VERSION " (" ARCH "/" OS ")" ;
static char compiled[]= __TIME__ __DATE__ ;
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
    -P file      create a pid file\n"
#ifdef STATS
"    -s file	 File to which statistics is dumped (disabled otherwise)\n"
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
int *pids=0;					/*array with childrens pids, 0= main proc,
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
/* use dns and/or rdns or to see if we need to add 
   a ;received=x.x.x.x to via: */
int received_dns = 0;      
char* working_dir = 0;
char* chroot_dir = 0;
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
struct socket_info sock_info[MAX_LISTEN]; /* all addresses we listen/send from*/
int sock_no=0; /* number of addresses/open sockets*/
struct socket_info* bind_address; /* pointer to the crt. proc. listening address */
int bind_idx; /* same as above but index in the bound[] array */
struct socket_info* sendipv4; /* ipv4 socket to use when msg. comes from ipv6*/
struct socket_info* sendipv6; /* same as above for ipv6 */

unsigned short port_no=0; /* default port*/

struct host_alias* aliases=0; /* name aliases list */

/* ipc related globals */
int process_no = 0;
process_bm_t process_bit = 0;
#ifdef ROUTE_SRV
#endif

/* cfg parsing */
int cfg_errors=0;

/* shared memory (in MB) */
unsigned int shm_mem_size=SHM_MEM_SIZE * 1024 * 1024;

#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse();


int is_main=0; /* flag = is this the  "main" process? */

char* pid_file = 0; /* filename as asked by use */

/* daemon init, return 0 on success, -1 on error */
int daemonize(char*  name)
{
	FILE *pid_stream;
	pid_t pid;
	int r, p;


	p=-1;

	if (log_stderr==0)
		openlog(name, LOG_PID|LOG_CONS, LOG_LOCAL1 /*LOG_DAEMON*/);
		/* LOG_CONS, LOG_PERRROR ? */


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
	
	/* close any open file descriptors */
	if (log_stderr==0)
		for (r=0;r<MAX_FD; r++){
			if ((r==3) && log_stderr)  continue;
			close(r);
		}
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
		case SIGINT:
		case SIGPIPE:
		case SIGTERM:
			/* we end the program in all these cases */
			if (sig_flag==SIGINT)
				DBG("INT received, program terminates\n");
			else if (sig_flag==SIGPIPE)
				DBG("SIGPIPE rreceived, program terminates\n");
			else
				DBG("SIGTERM received, program terminates\n");
				
			destroy_modules();
#ifdef PKG_MALLOC
			LOG(L_INFO, "Memory status (pkg):\n");
			pkg_status();
#endif
#ifdef SHM_MEM
			LOG(L_INFO, "Memory status (shm):\n");
			shm_status();
			/* zero all shmem alloc vars that we still use */
			pids=0;
			shm_mem_destroy();
#endif
			if (pid_file) unlink(pid_file);
			/* kill children also*/
			kill(0, SIGTERM);
			dprint("Thank you for flying " NAME "\n");
			exit(0);
			break;
			
		case SIGUSR1:
#ifdef STATS
			dump_all_statistic();
#endif
#ifdef PKG_MALLOC
			LOG(L_INFO, "Memory status (pkg):\n");
			pkg_status();
#endif
#ifdef SHM_MEM
			LOG(L_INFO, "Memory status (shm):\n");
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
			LOG(L_WARN, "WARNING: using only the first listen address (no fork)\n");
		}

		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main 
		*/

		/* we need another process to act as the timer*/
		if (timer_list){
				process_no++;
				if ((pid=fork())<0){
					LOG(L_CRIT,  "ERROR: main_loop: Cannot fork\n");
					goto error;
				}
				
				if (pid==0){
					/* child */
					/* timer!*/
					process_bit = 0;
					for(;;){
						sleep(TIMER_TICK);
						timer_ticker();
					}
				}else{
						pids[process_no]=pid; /*should be shared mem anway*/
				}
		}

		/* if configured to do so, start a server for accepting FIFO commands */
		if (open_fifo_server()<0) {
			LOG(L_ERR, "opening fifo server failed\n");
			goto error;
		}
		/* main process, receive loop */
		pids[0]=getpid();
		process_bit = 1;
		process_no=0; /*main process number*/
		
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
		   as new processes are forked (while skipping 0 reserved for main ;
		   not that with multiple listeners, more children processes will
		   share the same process_no and the pids array will be rewritten
		*/
		for(r=0;r<sock_no;r++){
			/* create the listening socket (for each address)*/
			if (udp_init(&sock_info[r])==-1) goto error;
			/* get first ipv4/ipv6 socket*/
			if ((sendipv4==0)&&(sock_info[r].address.af==AF_INET))
				sendipv4=&sock_info[r];
	#ifdef USE_IPV6
			if((sendipv6==0)&&(sock_info[r].address.af==AF_INET6))
				sendipv6=&sock_info[r];
	#endif
			/* all procs should have access to all the sockets (for sending)
			 * so we open all first*/
		}
		for(r=0; r<sock_no;r++){
			for(i=0;i<children_no;i++){
				if ((pid=fork())<0){
					LOG(L_CRIT,  "main_loop: Cannot fork\n");
					goto error;
				}else if (pid==0){
					     /* child */
					bind_address=&sock_info[r]; /* shortcut */
					bind_idx=r;
					if (init_child(i) < 0) {
						LOG(L_ERR, "init_child failed\n");
						goto error;
					}

					process_no=i+1; /*0=main*/
					process_bit = 1 << i;
#ifdef STATS
					setstats( i );
#endif
					return udp_rcv_loop();
				}else{
						pids[i+1]=pid; /*should be in shared mem.*/
				}
			}
			/*parent*/
			/*close(udp_sock)*/; /*if it's closed=>sendto invalid fd errors?*/
		}
	}
	/* process_no is still at zero ... it was only updated in children */ 
	process_no=children_no;

	/*this is the main process*/
	bind_address=&sock_info[0]; /* main proc -> it shoudln't send anything, */
	bind_idx=0;					/* if it does it will use the first address */
	/* if configured to do so, start a server for accepting FIFO commands */
	if (open_fifo_server()<0) {
		LOG(L_ERR, "opening fifo server failed\n");
		goto error;
	}

	if (timer_list){
		/* fork again for the attendant process*/
		process_no++;
		if ((pid=fork())<0){
			LOG(L_CRIT, "main_loop: cannot fork timer process\n");
			goto error;
		}else if (pid==0){
			/* child */
			/* is_main=0; */ /* warning: we don't keep this process pid*/
			for(;;){
				/* debug:  instead of doing something usefull */
				/* (placeholder for timers, etc.) */
				sleep(TIMER_TICK);
				/* if we received a signal => TIMER_TICK may have not elapsed*/
				timer_ticker();
			}
		}
	}

	process_no=0; /* main */
	pids[process_no]=getpid();
	process_bit = 0;
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
					/* print memory stats for non-main too */
					#ifdef PKG_MALLOC
					LOG(L_INFO, "Memory status (pkg):\n");
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
	struct ifreq* ifr;
	struct ifreq ifrcopy;
	char*  last;
	int size;
	int lastlen;
	int s;
	char* tmp;
	struct ip_addr addr;
	int ret;

#ifdef __FreeBSD__
	#define MAX(a,b) ( ((a)>(b))?(a):(b))
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
	for(ifr=ifc.ifc_req; (char*)ifr<last;
			ifr=(struct ifreq*)((char*)ifr+sizeof(ifr->ifr_name)+
			#ifdef  __FreeBSD__
				MAX(ifr->ifr_addr.sa_len, sizeof(struct sockaddr))
			#else
				( (ifr->ifr_addr.sa_family==AF_INET)?
					sizeof(struct sockaddr_in):
					((ifr->ifr_addr.sa_family==AF_INET6)?
						sizeof(struct sockaddr_in6):sizeof(struct sockaddr)) )
			#endif
				)
		)
	{
		if (ifr->ifr_addr.sa_family!=family){
			/*printf("strange family %d skipping...\n",
					ifr->ifr_addr.sa_family);*/
			continue;
		}
		
		if (if_name==0){ /* ignore down ifs only if listening on all of them*/
			memcpy(&ifrcopy, ifr, sizeof(ifrcopy));
			/*get flags*/
			if (ioctl(s, SIOCGIFFLAGS,  &ifrcopy)!=-1){ /* ignore errors */
				/* if if not up, skip it*/
				if (!(ifrcopy.ifr_flags & IFF_UP)) continue;
			}
		}
		
		
		
		if ((if_name==0)||
			(strncmp(if_name, ifr->ifr_name, sizeof(ifr->ifr_name))==0)){
			
				/*add address*/
			if (sock_no<MAX_LISTEN){
				sockaddr2ip_addr(&addr, &ifr->ifr_addr);
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
	int port_no_str_len=0;

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
	"f:p:m:b:l:n:rRvdDEVhw:t:u:g:P:";
	
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
					uid=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad uid number: -u %s\n", optarg);
						goto error;
					}
					/* test if string?*/
					break;
			case 'g':
					gid=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad gid number: -g %s\n", optarg);
						goto error;
					}
					break;
			case 'P':
					pid_file=optarg;
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


        init_hfname_parser();
	init_digest_parser();

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
#ifdef _OBSOLETED
	else if (children_no >= MAX_PROCESSES ) {
		fprintf(stderr, "ERROR: too many children processes configured;"
				" maximum is %d\n",
			MAX_PROCESSES-1 );
		goto error;
	}
#endif
	
	if (working_dir==0) working_dir="/";
	/*alloc pids*/
#ifdef SHM_MEM
	pids=shm_malloc(sizeof(int)*(children_no+1/*timer */+1/*fifo*/));
#else
	pids=malloc(sizeof(int)*(children_no+1));
#endif
	if (pids==0){
		fprintf(stderr, "ERROR: out  of memory\n");
		goto error;
	}
	memset(pids, 0, sizeof(int)*(children_no+1));

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
	printf("Listening on \n");
	for (r=0; r<sock_no;r++){
		he=resolvehost(sock_info[r].name.s);
		if (he==0){
			DPrint("ERROR: could not resolve %s\n", sock_info[r].name.s);
			goto error;
		}
		/* check if we got the official name */
		if (strcasecmp(he->h_name, sock_info[r].name.s)!=0){
			if (add_alias(sock_info[r].name.s, sock_info[r].name.len)<0){
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
			if (add_alias(*h, strlen(*h))<0){
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
			)	sock_info[r].is_ip=1;
		else sock_info[r].is_ip=0;
			
		if (sock_info[r].port_no==0) sock_info[r].port_no=port_no;
		port_no_str_len=snprintf(port_no_str, MAX_PORT_LEN, ":%d", 
									(unsigned short) sock_info[r].port_no);
		if (port_no_str_len<0){
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
		strncpy(sock_info[r].port_no_str.s, port_no_str, strlen(port_no_str)+1);
		sock_info[r].port_no_str.len=strlen(port_no_str);
		
		printf("              %s [%s]:%s\n",sock_info[r].name.s,
				sock_info[r].address_str.s, sock_info[r].port_no_str.s);
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
				printf("removing duplicate (%d) %s [%s] == (%d) %s [%s]\n",
						r, sock_info[r].name.s, sock_info[r].address_str.s,
						t, sock_info[t].name.s, sock_info[t].address_str.s);
				
				/* add the name to the alias list*/
				if ((!sock_info[t].is_ip) && (
						(sock_info[t].name.len!=sock_info[r].name.len)||
						(strncmp(sock_info[t].name.s, sock_info[r].name.s,
								 sock_info[r].name.len)!=0))
					)
					add_alias(sock_info[t].name.s, sock_info[t].name.len);
						
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
	for(a=aliases; a; a=a->next) printf("%.*s ", a->alias.len, a->alias.s);
	printf("\n");


	
	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize(argv[0]) <0 ) goto error;
	}
	if (init_modules() != 0) {
		fprintf(stderr, "ERROR: error while initializing modules\n");
		goto error;
	}
	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		fprintf(stderr, "ERROR: error %x while trying to fix configuration\n",
						r);
		goto error;
	};

#ifdef STATS
	if (init_stats(  dont_fork ? 1 : children_no  )==-1) goto error;
#endif
	
	return main_loop();


error:
	return -1;

}

