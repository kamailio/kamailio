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
#include "msg_parser.h"


#include <signal.h>

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
#ifdef SILENT_FR
", SILENT_FR"
#endif
#ifdef USE_SYNONIM
", USE_SYNONIM"
#endif
#ifdef NOISY_REPLIES
", NOISY_REPLIES"
#endif
#ifdef VERY_NOISY_REPLIES
", VERY_NOISY_REPLIES"
#endif
#ifdef NEW_HNAME
", NEW_HNAME"
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
Usage: " NAME " -l address [-l address] [options]\n\
Options:\n\
    -c           Perform loop checks and compute branches\n\
    -f file      Configuration file (default " CFG_FILE ")\n\
    -p port      Listen on the specified port (default: 5060)\n\
    -l address   Listen on the specified address (multiple -l mean\n\
                 listening on more addresses). The default behaviour\n\
                 is to listen on the addresses returned by uname(2)\n\
\n\
    -n processes Number of child processes to fork per interface\n\
                 (default: 8)\n\
\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                 field to a via\n\
    -R           Same as `-r´ but use reverse dns;\n\
                 (to use both use `-rR´)\n\
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
			" MAX_URI_SIZE %d, MAX_PROCESSES %d\n",
		MAX_RECV_BUFFER_SIZE, MAX_LISTEN, MAX_URI_SIZE, MAX_PROCESSES );
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
unsigned short port_no = 0; /* port on which we listen */
char port_no_str[MAX_PORT_LEN];
int port_no_str_len=0;
unsigned int maxbuffer = MAX_RECV_BUFFER_SIZE; /* maximum buffer size we do not want to exceed
				      		durig the auto-probing procedure; may be
				      		re-configured */
int children_no = 0;           /* number of children processing requests */
int *pids=0;		       /*array with childrens pids, 0= main proc,
				alloc'ed in shared mem if possible*/
int debug = 0;
int dont_fork = 0;
int log_stderr = 0;
int check_via =  0;        /* check if reply first via host==us */
int loop_checks = 0;	/* calculate branches and check for loops/spirals */
int received_dns = 0;      /* use dns and/or rdns or to see if we need to 
                              add a ;received=x.x.x.x to via: */
char* working_dir = 0;
char* chroot_dir = 0;
int uid = 0;
int gid = 0;

char* names[MAX_LISTEN];              /* our names */
int names_len[MAX_LISTEN];            /* lengths of the names*/
unsigned int addresses[MAX_LISTEN];   /* our ips */
int addresses_no=0;                   /* number of names/ips */
unsigned int bind_address=0;          /* listen address of the crt. process */

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


static int is_main=0; /* flag = is this the  "main" process? */

char* pid_file = 0;

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
	}
	if (pid!=0){
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
	}
	if (pid!=0){
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
			LOG(L_WARN, "unable to create pid file: %s\n", strerror(errno));
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



/* main loop */
int main_loop()
{
	int r, i;
	pid_t pid;

	/* one "main" process and n children handling i/o */


	if (dont_fork){
#ifdef STATS
		setstats( 0 );
#endif
		/* only one address */
		if (udp_init(addresses[0],port_no)==-1) goto error;

		/* we need another process to act as the timer*/
		if (timer_list){
				process_no++;
				if ((pid=fork())<0){
					LOG(L_CRIT,  "ERRROR: main_loop: Cannot fork\n");
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
		/* main process, receive loop */
		is_main=1;
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
		
		return udp_rcv_loop();
	}else{
		for(r=0;r<addresses_no;r++){
			/* create the listening socket (for each address)*/
			if (udp_init(addresses[r], port_no)==-1) goto error;
			for(i=0;i<children_no;i++){
				if ((pid=fork())<0){
					LOG(L_CRIT,  "main_loop: Cannot fork\n");
					goto error;
				}
				if (pid==0){
					     /* child */

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
	/*this is the main process*/
	pids[process_no]=getpid();
	process_bit = 0;
	is_main=1;
	if (timer_list){
		for(;;){
			/* debug:  instead of doing something usefull */
			/* (placeholder for timers, etc.) */
			sleep(TIMER_TICK);
			/* if we received a signal => TIMER_TICK may have not elapsed*/
			timer_ticker();
		}
	}else{
		for(;;) sleep(LONG_SLEEP);
	}
	
	/*return 0; */
 error:
	return -1;

}


/* added by jku; allows for regular exit on a specific signal;
   good for profiling which only works if exited regularly and
   not by default signal handlers
*/	

static void sig_usr(int signo)
{
	pid_t	chld;
	int	chld_status;

	if (signo==SIGINT || signo==SIGPIPE) {	/* exit gracefuly */
		DPrint("INT received, program terminates\n");
#		ifdef STATS
		/* print statistics on exit only for the first process */
		if (stats->process_index==0 && stat_file )
			if (dump_all_statistic()==0)
				printf("statistic dumped to %s\n", stat_file );
			else
				printf("statistics dump to %s failed\n", stat_file );
#		endif
		/* WARNING: very dangerous, might be unsafe*/
		if (is_main)
			destroy_modules();
#ifdef PKG_MALLOC
		LOG(L_INFO, "Memory status (pkg):\n");
		pkg_status();
#		endif
#ifdef SHM_MEM
		if (is_main){
			LOG(L_INFO, "Memory status (shm):\n");
			shm_status();
			/*zero all shmem  alloc vars, that will still use*/
			pids=0;
			shm_mem_destroy();
		}
#endif
		dprint("Thank you for flying " NAME "\n");
		exit(0);
	} else if (signo==SIGTERM) { /* exit gracefully as daemon */
		DPrint("TERM received, program terminates\n");
		if (is_main){
#ifdef STATS
			dump_all_statistic();
#endif
			if (pid_file) unlink(pid_file);
		}
		exit(0);
	} else if (signo==SIGUSR1) { /* statistic */
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
	} else if (signo==SIGCHLD) {
		while ((chld=waitpid( -1, &chld_status, WNOHANG ))>0) {
			if (WIFEXITED(chld_status)) 
				LOG(L_INFO, "child process %d exited normally, status=%d\n",
					chld, WEXITSTATUS(chld_status));
			else if (WIFSIGNALED(chld_status)) {
				LOG(L_INFO, "child process %d exited by a signal %d\n",
					chld, WTERMSIG(chld_status));
#				ifdef WCOREDUMP
				LOG(L_INFO, "core was %sgenerated\n", WCOREDUMP(chld_status) ?
					"" : "not" );
#				endif
			} else if (WIFSTOPPED(chld_status)) 
				LOG(L_INFO, "child process %d stopped by a signal %d\n",
					chld, WSTOPSIG(chld_status));
		}
	}
}


void test();

int main(int argc, char** argv)
{

	FILE* cfg_stream;
	struct hostent* he;
	int c,r;
	char *tmp;
	struct utsname myname;
	char *options;

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

	//memtest();
	//hashtest();

	/* process command line (get port no, cfg. file path etc) */
	opterr=0;
	options=
#ifdef STATS
	"s:"
#endif
	"f:p:m:b:l:n:rRvcdDEVhw:t:u:g:P:";
	
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
					break;

			case 'm':
					shm_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad shmem size number: -m %s\n", optarg);
						goto error;
					};
					LOG(L_INFO, "ser: shared memory allocated: %d MByte\n", shm_mem_size );
					break;

			case 'b':
					maxbuffer=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
                                                fprintf(stderr, "bad max buffer size number: -p %s\n", optarg);
                                                goto error;
                                        }
                                        break;
			case 'l':
					/* add a new addr. to our address list */
					if (addresses_no < MAX_LISTEN){
						names[addresses_no]=(char*)malloc(strlen(optarg)+1);
						if (names[addresses_no]==0){
							fprintf(stderr, "Out of memory.\n");
							goto error;
						}
						strncpy(names[addresses_no], optarg, strlen(optarg)+1);
						addresses_no++;
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
						fprintf(stderr, "bad process number: -n %s\n", optarg);
						goto error;
					}
					break;
			case 'v':
					check_via=1;
					break;
			case 'c':
					loop_checks=1;
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

#ifdef NEW_HNAME
    init_htable();
#endif

	/*init mallocs (before parsing cfg !)*/
	if (init_mallocs()==-1)
		goto error;

	/*init timer, before parsing the cfg!*/
	if (init_timer()<0){
		LOG(L_CRIT, "could not initialize timer, exiting...\n");
		goto error;
	}

	/*register builtin  modules*/
	register_builtin_modules();

	yyin=cfg_stream;
	if ((yyparse()!=0)||(cfg_errors)){
		fprintf(stderr, "ERROR: bad config file (%d errors)\n", cfg_errors);
		goto error;
	}
	
	if (init_modules() != 0) {
		fprintf(stderr, "ERROR: error while initializing modules\n");
		goto error;
	}

	print_rl();
	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		fprintf(stderr, "ERROR: error %x while trying to fix configuration\n",
						r);
		goto error;
	};

	/* fix parameters */
	if (port_no<=0) port_no=SIP_PORT;
	port_no_str_len=snprintf(port_no_str, MAX_PORT_LEN, ":%d", 
				(unsigned short) port_no);
	if (port_no_str_len<0){
		fprintf(stderr, "ERROR: bad port number: %d\n", port_no);
		goto error;
	}
	/* on some system snprintf return really strange things if it does not have
	 * enough space */
	port_no_str_len=
				(port_no_str_len<MAX_PORT_LEN)?port_no_str_len:MAX_PORT_LEN;

	
	if (children_no<=0) children_no=CHILD_NO;
	else if (children_no >= MAX_PROCESSES ) {
		fprintf(stderr, "ERROR: too many children processes configured;"
				" maximum is %d\n",
			MAX_PROCESSES-1 );
		goto error;
	}
	
	if (working_dir==0) working_dir="/";
	/*alloc pids*/
#ifdef SHM_MEM
	pids=shm_malloc(sizeof(int)*(children_no+1));
#else
	pids=malloc(sizeof(int)*(children_no+1));
#endif
	if (pids==0){
		fprintf(stderr, "ERROR: out  of memory\n");
		goto error;
	}
	memset(pids, 0, sizeof(int)*(children_no+1));

	if (addresses_no==0) {
		/* get our address, only the first one */
		if (uname (&myname) <0){
			fprintf(stderr, "cannot determine hostname, try -l address\n");
			goto error;
		}
		names[addresses_no]=(char*)malloc(strlen(myname.nodename)+1);
		if (names[addresses_no]==0){
			fprintf(stderr, "Out of memory.\n");
			goto error;
		}
		strncpy(names[addresses_no], myname.nodename,
				strlen(myname.nodename)+1);
		addresses_no++;
	}

	/*get name lens*/
	for(r=0; r<addresses_no; r++){
		names_len[r]=strlen(names[r]);
	}

	
	/* get ips */
	printf("Listening on ");
	for (r=0; r<addresses_no;r++){
		he=gethostbyname(names[r]);
		if (he==0){
			DPrint("ERROR: could not resolve %s\n", names[r]);
			goto error;
		}
		memcpy(&addresses[r], he->h_addr_list[0], sizeof(int));
		/*addresses[r]=*((long*)he->h_addr_list[0]);*/
		printf("%s [%s] : %d\n",names[r],
				inet_ntoa(*(struct in_addr*)&addresses[r]),
				(unsigned short)port_no);
	}

#ifdef STATS
	if (init_stats(  dont_fork ? 1 : children_no  )==-1) goto error;
#endif

	
	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize(argv[0]) <0 ) goto error;
	}

	return main_loop();


error:
	return -1;

}

