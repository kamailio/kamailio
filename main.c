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

#include "config.h"
#include "dprint.h"
#include "route.h"
#include "udp_server.h"
#include "globals.h"

#include <signal.h>



#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif


static char id[]="@(#) $Id$";
static char version[]="ser 0.7";
static char flags[]="NOCR:"
#ifdef NOCR
"On"
#else
"Off"
#endif
", MACROEATER:"
#ifdef MACROEATER
"On"
#else
"Off"
#endif
;

static char help_msg[]= "\
Usage: ser -l address [-l address] [options]\n\
Options:\n\
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
";


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
int children_no = 0;           /* number of children processing requests */
int debug = 0;
int dont_fork = 0;
int log_stderr = 0;
int check_via =  0;        /* check if reply first via host==us */
int received_dns = 0;      /* use dns and/or rdns or to see if we need to 
                              add a ;received=x.x.x.x to via: */

char* names[MAX_LISTEN];               /* our names */
unsigned long addresses[MAX_LISTEN];   /* our ips */
int addresses_no=0;                    /* number of names/ips */

/* ipc related globals */
int process_no = 0;
#ifdef ROUTE_SRV
#endif

/* cfg parsing */
int cfg_errors=0;


#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse();



/* daemon init, return 0 on success, -1 on error */
int daemonize(char*  name)
{
	pid_t pid;
	int r;
	
	if (log_stderr==0)
		openlog(name, LOG_PID, LOG_DAEMON); /* LOG_CONS, LOG_PERRROR ? */

	if (chdir("/")<0){
		LOG(L_CRIT,"cannot chroot:%s\n", strerror(errno));
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
	
	/* close any open file descriptors */
	
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
		/* only one address */
		if (udp_init(addresses[0],port_no)==-1) goto error;
		/* receive loop */
		udp_rcv_loop();
	}else{
		for(r=0;r<addresses_no;r++){
			for(i=0;i<children_no;i++){
				if ((pid=fork())<0){
					LOG(L_CRIT,  "main_loop: Cannot fork\n");
					goto error;
				}
				if (pid==0){
					/* child */
					if (udp_init(addresses[r], port_no)==-1) goto error;
					return udp_rcv_loop();
				}
			}
		}
	}
		
	for(;;){
		/* debug:  instead of select */
		sleep(1);
	}
	
	return 0;
 error:
	return -1;

}

/* added by jku; allows for regular exit on a specific signal;
   good for profiling which only works if exited regularly and
   not by default signal handlers
*/	

static void sig_usr(int signo)
{
	DPrint("INT received, program terminates\n");
	exit(0);
}
	
	
	
int main(int argc, char** argv)
{

	FILE* cfg_stream;
	struct hostent* he;
	int c,r;
	char *tmp;
	struct utsname myname;

	/* added by jku: add exit handler */
        if (signal(SIGINT, sig_usr) == SIG_ERR ) {
 		DPrint("ERROR: no signal handler can be installed\n");
                goto error;
        }

	/* process command line (get port no, cfg. file path etc) */
	opterr=0;
	while((c=getopt(argc,argv,"f:p:l:n:rRvdDEVh"))!=-1){
		switch(c){
			case 'f':
					cfg_file=optarg;
					break;
			case 'p':
					port_no=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad port number: -p %s\n", optarg);
						goto error;
					}
					break;
			case 'l':
					/* add a new addr. to out address list */
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
					printf("%s\n",id);
					exit(0);
					break;
			case 'h':
					printf("version: %s\n", version);
					printf("%s",help_msg);
					exit(0);
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
	
	yyin=cfg_stream;
	if ((yyparse()!=0)||(cfg_errors)){
		fprintf(stderr, "ERROR: bad config file (%d errors)\n", cfg_errors);
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
	if (children_no<=0) children_no=CHILD_NO;
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
	
	/* get ips */
	printf("Listening on ");
	for (r=0; r<addresses_no;r++){
		he=gethostbyname(names[r]);
		if (he==0){
			DPrint("ERROR: could not resolve %s\n", names[r]);
			goto error;
		}
		addresses[r]=*((long*)he->h_addr_list[0]);
		printf("%s [%s] : %d\n",names[r],
				inet_ntoa(*(struct in_addr*)&addresses[r]),
				(unsigned short)port_no);
	}

	/* init_daemon? */
	if (!dont_fork){
		if ( daemonize(argv[0]) <0 ) goto error;
	}
		
	
	return main_loop();


error:
	return -1;

}
