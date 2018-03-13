/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/** Kamailio core :: main file (init, daemonize, startup)
 * @file main.c
 * @ingroup core
 * Module: core
 */

/*! @defgroup core Kamailio core
 *
 * sip router core part.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#if defined(HAVE_NETINET_IN_SYSTM)
#include <netinet/in_systm.h>
#endif
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
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

#include "core/config.h"
#include "core/dprint.h"
#include "core/daemonize.h"
#include "core/route.h"
#include "core/udp_server.h"
#include "core/globals.h"
#include "core/mem/mem.h"
#ifdef SHM_MEM
#include "core/mem/shm_mem.h"
#include "core/shm_init.h"
#endif /* SHM_MEM */
#include "core/sr_module.h"
#include "core/timer.h"
#include "core/parser/msg_parser.h"
#include "core/ip_addr.h"
#include "core/resolve.h"
#include "core/parser/parse_hname2.h"
#include "core/parser/digest/digest_parser.h"
#include "core/name_alias.h"
#include "core/hash_func.h"
#include "core/pt.h"
#include "core/script_cb.h"
#include "core/nonsip_hooks.h"
#include "core/ut.h"
#include "core/events.h"
#include "core/signals.h"
#ifdef USE_RAW_SOCKS
#include "core/raw_sock.h"
#endif /* USE_RAW_SOCKS */
#ifdef USE_TCP
#include "core/poll_types.h"
#include "core/tcp_init.h"
#include "core/tcp_options.h"
#ifdef CORE_TLS
#include "core/tls/tls_init.h"
#define tls_has_init_si() 1
#define tls_loaded() 1
#else
#include "core/tls_hooks_init.h"
#endif /* CORE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
#include "core/sctp_core.h"
#endif
#include "core/usr_avp.h"
#include "core/rpc_lookup.h"
#include "core/core_cmd.h"
#include "core/flags.h"
#include "core/lock_ops_init.h"
#include "core/atomic_ops_init.h"
#ifdef USE_DNS_CACHE
#include "core/dns_cache.h"
#endif
#ifdef USE_DST_BLACKLIST
#include "core/dst_blacklist.h"
#endif
#include "core/rand/fastrand.h" /* seed */
#include "core/rand/kam_rand.h"

#include "core/stats.h"
#include "core/counters.h"
#include "core/cfg/cfg.h"
#include "core/cfg/cfg_struct.h"
#include "core/cfg_core.h"
#include "core/endianness.h" /* init */
#include "core/basex.h" /* init */
#include "core/pvapi.h" /* init PV api */
#include "core/pv_core.h" /* register core pvars */
#include "core/ppcfg.h"
#include "core/sock_ut.h"
#include "core/async_task.h"
#include "core/dset.h"
#include "core/timer_proc.h"
#include "core/srapi.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif
#include "core/ver.h"

/* define SIG_DEBUG by default */
#ifdef NO_SIG_DEBUG
#undef SIG_DEBUG
#else
#define SIG_DEBUG
#endif



static char help_msg[]= "\
Usage: " NAME " [options]\n\
Options:\n\
    -f file      Configuration file (default: " CFG_FILE ")\n\
    -L path      Modules search path (default: " MODS_DIR ")\n\
    -c           Check configuration file for errors\n\
    -l address   Listen on the specified address/interface (multiple -l\n\
                  mean listening on more addresses).  The address format is\n\
                  [proto:]addr_lst[:port], where proto=udp|tcp|tls|sctp, \n\
                  addr_lst= addr|(addr, addr_lst) and \n\
                  addr= host|ip_address|interface_name. \n\
                  E.g: -l localhost, -l udp:127.0.0.1:5080, -l eth0:5062,\n\
                  -l \"sctp:(eth0)\", -l \"(eth0, eth1, 127.0.0.1):5065\".\n\
                  The default behaviour is to listen on all the interfaces.\n\
    -n processes Number of child processes to fork per interface\n\
                  (default: 8)\n\
    -r           Use dns to check if is necessary to add a \"received=\"\n\
                  field to a via\n\
    -R           Same as `-r` but use reverse dns;\n\
                  (to use both use `-rR`)\n\
    -K           Turn on \"via:\" host checking when forwarding replies\n\
    -d           Debugging mode (multiple -d increase the level)\n\
    -D           Control how daemonize is done:\n\
                  -D..do not fork (almost) anyway;\n\
                  -DD..do not daemonize creator;\n\
                  -DDD..daemonize (default)\n\
    -E           Log to stderr\n\
    -e           Log messages printed in terminal colors (requires -E)\n"
#ifdef USE_TCP
"    -T           Disable tcp\n\
    -N           Number of tcp child processes (default: equal to `-n')\n\
    -W type      poll method (depending on support in OS, it can be: poll,\n\
                  epoll_lt, epoll_et, sigio_rt, select, kqueue, /dev/poll)\n"
#endif
#ifdef USE_SCTP
"    -S           disable sctp\n\
    -Q            Number of sctp child processes (default: equal to `-n')\n"
#endif /* USE_SCTP */
"    -v (-V)      Version number\n\
    -h           This help message\n\
    -I           Print more internal compile flags and options\n\
    -b nr        Maximum receive buffer size which will not be exceeded by\n\
                  auto-probing procedure even if  OS allows\n\
    -m nr        Size of shared memory allocated in Megabytes\n\
    -M nr        Size of private memory allocated, in Megabytes\n\
    -w dir       Change the working directory to \"dir\" (default: \"/\")\n\
    -t dir       Chroot to \"dir\"\n\
    -u uid       Change uid (user id)\n\
    -g gid       Change gid (group id)\n\
    -P file      Create a pid file\n\
    -G file      Create a pgid file\n\
    -Y dir       Runtime dir path\n\
    -O nr        Script optimization level (debugging option)\n\
    -a mode      Auto aliases mode: enable with yes or on,\n\
                  disable with no or off\n\
    -A define    Add config pre-processor define (e.g., -A WITH_AUTH)\n\
    -x name      Specify internal manager for shared memory (shm)\n\
                  - can be: fm, qm or tlsf\n\
    -X name      Specify internal manager for private memory (pkg)\n\
                  - if omitted, the one for shm is used\n"
#ifdef STATS
"    -s file     File where to write internal statistics on SIGUSR1\n"
#endif
;


/* print compile-time constants */
void print_ct_constants(void)
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
			" MAX_URI_SIZE %d, BUF_SIZE %d, DEFAULT PKG_SIZE %uMB\n",
		MAX_RECV_BUFFER_SIZE, MAX_LISTEN, MAX_URI_SIZE,
		BUF_SIZE, PKG_MEM_SIZE);
#ifdef USE_TCP
	printf("poll method support: %s.\n", poll_support);
#endif
}

/* print compile-time constants */
void print_internals(void)
{
	printf("Print out of %s internals\n", NAME);
	printf("  Version: %s\n", full_version);
	printf("  Default config: %s\n", CFG_FILE);
	printf("  Default paths to modules: %s\n", MODS_DIR);
	printf("  Compile flags: %s\n", ver_flags );
	printf("  MAX_RECV_BUFFER_SIZE=%d\n", MAX_RECV_BUFFER_SIZE);
	printf("  MAX_LISTEN=%d\n", MAX_LISTEN);
	printf("  MAX_URI_SIZE=%d\n", MAX_URI_SIZE);
	printf("  BUF_SIZE=%d\n", BUF_SIZE);
	printf("  DEFAULT PKG_SIZE=%uMB\n", PKG_MEM_SIZE);
#ifdef SHM_MEM
	printf("  DEFAULT SHM_SIZE=%uMB\n", SHM_MEM_SIZE);
#endif
#ifdef ADAPTIVE_WAIT
	printf("  ADAPTIVE_WAIT_LOOPS=%d\n", ADAPTIVE_WAIT_LOOPS);
#endif
#ifdef USE_TCP
	printf("  TCP poll methods: %s\n", poll_support);
#endif
	printf("  Source code revision ID: %s\n", ver_id);
	printf("  Compiled with: %s\n", ver_compiler);
	printf("  Compiled on: %s\n", ver_compiled_time);
	printf("Thank you for flying %s!\n", NAME);
}

/* debugging function */
/*
void receive_stdin_loop(void)
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

char* mods_dir = MODS_DIR;  /* search path for dyn. loadable modules */
int   mods_dir_cmd = 0; /* mods dir path set in command lin e*/

char* cfg_file = 0;
unsigned int maxbuffer = MAX_RECV_BUFFER_SIZE; /* maximum buffer size we do
												  not want to exceed during the
												  auto-probing procedure; may
												  be re-configured */
unsigned int sql_buffer_size = 65535; /* Size for the SQL buffer. Defaults to 64k. 
                                         This may be re-configured */
int socket_workers = 0;		/* number of workers processing requests for a socket
							   - it's reset everytime with a new listen socket */
int children_no = 0;		/* number of children processing requests */
#ifdef USE_TCP
int tcp_cfg_children_no = 0; /* set via config or command line option */
int tcp_children_no = 0; /* based on socket_workers and tcp_cfg_children_no */
int tcp_disable = 0; /* 1 if tcp is disabled */
#endif
#ifdef USE_TLS
#ifdef	CORE_TLS
int tls_disable = 0;  /* tls enabled by default */
#else
int tls_disable = 1;  /* tls disabled by default */
#endif /* CORE_TLS */
#endif /* USE_TLS */
#ifdef USE_SCTP
int sctp_children_no = 0;
int sctp_disable = 2; /* 1 if sctp is disabled, 2 if auto mode, 0 enabled */
#endif /* USE_SCTP */

struct process_table *pt=0;		/*array with children pids, 0= main proc,
									alloc'ed in shared mem if possible*/
int *process_count = 0;			/* Total number of SER processes currently
								   running */
gen_lock_t* process_lock;		/* lock on the process table */
int process_no = 0;				/* index of process in the pt */

time_t up_since;
int sig_flag = 0;              /* last signal received */
int dont_fork = 0;
int dont_daemonize = 0;
int log_stderr = 0;
int log_color = 0;
/* set custom app name for syslog printing */
char *log_name = 0;
char *log_prefix_fmt = 0;
pid_t creator_pid = (pid_t) -1;
int config_check = 0;
/* check if reply first via host==us */
int check_via =  0;
/* translate user=phone URIs to TEL URIs */
int phone2tel = 1;
/* debugging level for timer debugging */
int timerlog = L_WARN;
/* should replies include extensive warnings? by default no,
   good for trouble-shooting
*/
int sip_warning = 0;
/* should localy-generated messages include server's signature?
   be default yes, good for trouble-shooting
*/
int server_signature=1;
str server_hdr = {SERVER_HDR, SERVER_HDR_LEN};
str user_agent_hdr = {USER_AGENT, USER_AGENT_LEN};
str version_table = {VERSION_TABLE, VERSION_TABLE_LEN};
/* should ser try to locate outbound interface on multihomed
 * host? by default not -- too expensive
 */
int mhomed=0;
/* use dns and/or rdns or to see if we need to add
   a ;received=x.x.x.x to via: */
int received_dns = 0;
/* add or not the rev dns names to aliases list */
int sr_auto_aliases=1;
char* working_dir = 0;
char* chroot_dir = 0;
char* runtime_dir = "" RUN_DIR;
char* user=0;
char* group=0;
int uid = 0;
int gid = 0;
char* sock_user=0;
char* sock_group=0;
int sock_uid= -1;
int sock_gid= -1;
int sock_mode= S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP; /* rw-rw---- */

int server_id = 0; /* Configurable unique ID of the server */

/* maximum number of branches for transaction */
unsigned int sr_dst_max_branches = MAX_BRANCHES_DEFAULT;

/* set timeval for each received sip message */
int sr_msg_time = 1;

/* onsend_route is executed for replies*/
int onsend_route_reply = 0;

/* more config stuff */
int disable_core_dump=0; /* by default enabled */
int open_files_limit=-1; /* don't touch it by default */

/* memory options */
int shm_force_alloc=0; /* force immediate (on startup) page allocation
						  (by writting 0 in the pages), useful if
						  mlock_pages is also 1 */
int mlock_pages=0; /* default off, try to disable swapping */

/* real time options */
int real_time=0; /* default off, flags: 1 on only timer, 2  slow timer,
										4 all procs (7=all) */
int rt_prio=0;
int rt_policy=0; /* SCHED_OTHER */
int rt_timer1_prio=0;  /* "fast" timer */
int rt_timer2_prio=0;  /* "slow" timer */
int rt_timer1_policy=0; /* "fast" timer, SCHED_OTHER */
int rt_timer2_policy=0; /* "slow" timer, SCHED_OTHER */


/* a hint to reply modules whether they should send reply
   to IP advertised in Via or IP from which a request came
*/
int reply_to_via=0;

#ifdef USE_MCAST
int mcast_loopback = 0;
int mcast_ttl = -1; /* if -1, don't touch it, use the default (usually 1) */
char* mcast = 0;
#endif /* USE_MCAST */

int tos = IPTOS_LOWDELAY;
int pmtu_discovery = 0;

int auto_bind_ipv6 = 0;

#if 0
char* names[MAX_LISTEN];              /* our names */
int names_len[MAX_LISTEN];            /* lengths of the names*/
struct ip_addr addresses[MAX_LISTEN]; /* our ips */
int addresses_no=0;                   /* number of names/ips */
#endif
struct socket_info* udp_listen=0;
#ifdef USE_TCP
int tcp_main_pid=0; /* set after the tcp main process is started */
struct socket_info* tcp_listen=0;
#endif
#ifdef USE_TLS
struct socket_info* tls_listen=0;
#endif
#ifdef USE_SCTP
struct socket_info* sctp_listen=0;
#endif
struct socket_info* bind_address=0; /* pointer to the crt. proc.
									 listening address*/
struct socket_info* sendipv4; /* ipv4 socket to use when msg. comes from ipv6*/
struct socket_info* sendipv6; /* same as above for ipv6 */
#ifdef USE_RAW_SOCKS
int raw_udp4_send_sock = -1; /* raw socket used for sending udp4 packets */
#endif /* USE_RAW_SOCKS */
#ifdef USE_TCP
struct socket_info* sendipv4_tcp;
struct socket_info* sendipv6_tcp;
#endif
#ifdef USE_TLS
struct socket_info* sendipv4_tls;
struct socket_info* sendipv6_tls;
#endif
#ifdef USE_SCTP
struct socket_info* sendipv4_sctp;
struct socket_info* sendipv6_sctp;
#endif

unsigned short port_no=0; /* default port*/
#ifdef USE_TLS
unsigned short tls_port_no=0; /* default port */
#endif

struct host_alias* aliases=0; /* name aliases list */

/* Parameter to child_init */
int child_rank = 0;

/* how much to wait for children to terminate, before taking extreme measures*/
int ser_kill_timeout=DEFAULT_SER_KILL_TIMEOUT;

int ksr_verbose_startup = 0;

/* cfg parsing */
int cfg_errors=0;
int cfg_warnings=0;


/* shared memory (in MB) */
unsigned long shm_mem_size=0;
/* private (pkg) memory (in MB) */
unsigned long pkg_mem_size=0;

/* export command-line to anywhere else */
int my_argc;
char **my_argv;

/* set to 1 when the cfg framework and core cfg is initialized/registered */
static int cfg_ok=0;

#define MAX_FD 32 /* maximum number of inherited open file descriptors,
		    (normally it shouldn't  be bigger  than 3) */


extern FILE* yyin;
extern int yyparse(void);


int is_main=1; /* flag = is this the  "main" process? */
int fixup_complete=0; /* flag = is the fixup complete ? */

char* pid_file = 0; /* filename as asked by use */
char* pgid_file = 0;


/* memory manager */
#define SR_MEMMNG_DEFAULT	"qm"

char *sr_memmng_pkg = NULL;
char *sr_memmng_shm = NULL;

/* call it before exiting; if show_status==1, mem status is displayed */
void cleanup(int show_status)
{
	int memlog;

	/*clean-up*/
#ifndef SHM_SAFE_MALLOC
	if(shm_initialized()) {
		/* force-unlock the shared memory lock in case some process crashed
		 * and let it locked; this will allow an almost gracious shutdown */
		shm_global_unlock();
	}
#endif
	destroy_rpcs();
	destroy_modules();
#ifdef USE_DNS_CACHE
	destroy_dns_cache();
#endif
#ifdef USE_DST_BLACKLIST
	destroy_dst_blacklist();
#endif
	/* restore the original core configuration before the
	 * config block is freed, otherwise even logging is unusable,
	 * it can case segfault */
	if (cfg_ok){
		cfg_update();
		/* copy current config into default_core_cfg */
		if (core_cfg)
			default_core_cfg=*((struct cfg_group_core*)core_cfg);
	}
	core_cfg = &default_core_cfg;
	cfg_destroy();
#ifdef USE_TCP
	destroy_tcp();
#ifdef USE_TLS
	destroy_tls();
#endif /* USE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
	sctp_core_destroy();
#endif
	destroy_timer();
	pv_destroy_api();
	destroy_script_cb();
	destroy_nonsip_hooks();
	destroy_routes();
	destroy_atomic_ops();
	destroy_counters();
	memlog=cfg_get(core, core_cfg, memlog);
#ifdef PKG_MALLOC
	if (show_status && memlog <= cfg_get(core, core_cfg, debug)){
		if (cfg_get(core, core_cfg, mem_summary) & 1) {
			LOG(memlog, "Memory status (pkg):\n");
			pkg_status();
		}
		if (cfg_get(core, core_cfg, mem_summary) & 4) {
			LOG(memlog, "Memory still-in-use summary (pkg):\n");
			pkg_sums();
		}
	}
#endif
#ifdef SHM_MEM
	if (pt) shm_free(pt);
	pt=0;
	if (show_status && memlog <= cfg_get(core, core_cfg, debug)){
		if (cfg_get(core, core_cfg, mem_summary) & 2) {
			LOG(memlog, "Memory status (shm):\n");
			shm_status();
		}
		if (cfg_get(core, core_cfg, mem_summary) & 8) {
			LOG(memlog, "Memory still-in-use summary (shm):\n");
			shm_sums();
		}
	}
	/* zero all shmem alloc vars that we still use */
	shm_destroy_manager();
#endif
	destroy_lock_ops();
	if (pid_file) unlink(pid_file);
	if (pgid_file) unlink(pgid_file);
	pkg_destroy_manager();
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
	else if (pt){
		 /* lock processes table only if this is a child process
		  * (only main can add processes, so from main is safe not to lock
		  *  and moreover it avoids the lock-holding suicidal children problem)
		  */
		if (!is_main) lock_get(process_lock);
		for (r=1; r<*process_count; r++){
			if (r==process_no) continue; /* try not to be suicidal */
			if (pt[r].pid) {
				kill(pt[r].pid, signum);
			}
			else LM_CRIT("killing: %s > %d no pid!!!\n",
							pt[r].desc, pt[r].pid);
		}
		if (!is_main) lock_release(process_lock);
	}
}



/* if this handler is called, a critical timeout has occurred while
 * waiting for the children to finish => we should kill everything and exit */
static void sig_alarm_kill(int signo)
{
	kill_all_children(SIGKILL); /* this will kill the whole group
								  including "this" process;
								  for debugging replace with SIGABRT
								  (but warning: it might generate lots
								   of cores) */
}


/* like sig_alarm_kill, but the timeout has occurred when cleaning up
 * => try to leave a core for future diagnostics */
static void sig_alarm_abort(int signo)
{
	/* LOG is not signal safe, but who cares, we are abort-ing anyway :-) */
	LM_CRIT("shutdown timeout triggered, dying...");
	abort();
}



static void shutdown_children(int sig, int show_status)
{
	kill_all_children(sig);
	if (set_sig_h(SIGALRM, sig_alarm_kill) == SIG_ERR ) {
		LM_ERR("could not install SIGALARM handler\n");
		/* continue, the process will die anyway if no
		 * alarm is installed which is exactly what we want */
	}
	alarm(ser_kill_timeout);
	while((wait(0) > 0) || (errno==EINTR)); /* wait for all the
											   children to terminate*/
	set_sig_h(SIGALRM, sig_alarm_abort);
	cleanup(show_status); /* cleanup & show status*/
	alarm(0);
	set_sig_h(SIGALRM, SIG_IGN);
}



void handle_sigs(void)
{
	pid_t	chld;
	int	chld_status;
	int	any_chld_stopped;
	int memlog;

	switch(sig_flag){
		case 0: break; /* do nothing*/
		case SIGPIPE:
				/* SIGPIPE might be rarely received on use of
				   exec module; simply ignore it
				 */
				LM_WARN("SIGPIPE received and ignored\n");
				break;
		case SIGINT:
		case SIGTERM:
			/* we end the program in all these cases */
			if (sig_flag==SIGINT)
				LM_DBG("INT received, program terminates\n");
			else
				LM_DBG("SIGTERM received, program terminates\n");
			LM_NOTICE("Thank you for flying " NAME "!!!\n");
			/* shutdown/kill all the children */
			shutdown_children(SIGTERM, 1);
			exit(0);
			break;

		case SIGUSR1:
#ifdef STATS
			dump_all_statistic();
#endif
		memlog=cfg_get(core, core_cfg, memlog);
#ifdef PKG_MALLOC
		if (memlog <= cfg_get(core, core_cfg, debug)){
			if (cfg_get(core, core_cfg, mem_summary) & 1) {
				LOG(memlog, "Memory status (pkg):\n");
				pkg_status();
			}
			if (cfg_get(core, core_cfg, mem_summary) & 4) {
				LOG(memlog, "Memory still-in-use summary (pkg):\n");
				pkg_sums();
			}
		}
#endif
#ifdef SHM_MEM
		if (memlog <= cfg_get(core, core_cfg, debug)){
			if (cfg_get(core, core_cfg, mem_summary) & 2) {
				LOG(memlog, "Memory status (shm):\n");
				shm_status();
			}
			if (cfg_get(core, core_cfg, mem_summary) & 8) {
				LOG(memlog, "Memory still-in-use summary (shm):\n");
				shm_sums();
			}
		}
#endif
			break;

		case SIGCHLD:
			any_chld_stopped=0;
			while ((chld=waitpid( -1, &chld_status, WNOHANG ))>0) {
				any_chld_stopped=1;
				if (WIFEXITED(chld_status))
					LM_ALERT("child process %ld exited normally,"
							" status=%d\n", (long)chld,
							WEXITSTATUS(chld_status));
				else if (WIFSIGNALED(chld_status)) {
					LM_ALERT("child process %ld exited by a signal"
							" %d\n", (long)chld, WTERMSIG(chld_status));
#ifdef WCOREDUMP
					LM_ALERT("core was %sgenerated\n",
							 WCOREDUMP(chld_status) ?  "" : "not " );
#endif
				}else if (WIFSTOPPED(chld_status))
					LM_ALERT("child process %ld stopped by a"
								" signal %d\n", (long)chld,
								 WSTOPSIG(chld_status));
			}

			/* If it appears that no child process has stopped, then do not terminate on SIGCHLD.
			   Certain modules like app_python can run external scripts which cause child processes to be started and
			   stopped. That can result in SIGCHLD being received here even though there is no real problem. Therefore,
			   we do not terminate Kamailio unless we can find the child process which has stopped. */
			if (!any_chld_stopped) {
				LM_INFO("SIGCHLD received, but no child has stopped, ignoring it\n");
				break;
			}

#ifndef STOP_JIRIS_CHANGES
			if (dont_fork) {
				LM_INFO("dont_fork turned on, living on\n");
				break;
			}
			LM_INFO("terminating due to SIGCHLD\n");
#endif
			LM_DBG("terminating due to SIGCHLD\n");
			/* exit */
			shutdown_children(SIGTERM, 1);
			if (WIFSIGNALED(chld_status)) {
				exit(1);
			} else {
				exit(0);
			}
			break;

		case SIGHUP: /* ignoring it*/
					LM_DBG("SIGHUP received, ignoring it\n");
					break;
		default:
			LM_CRIT("unhandled signal %d\n", sig_flag);
	}
	sig_flag=0;
}



/* added by jku; allows for regular exit on a specific signal;
   good for profiling which only works if exited regularly and
   not by default signal handlers
    - modified by andrei: moved most of the stuff to handle_sigs,
       made it safer for the "fork" case
*/
void sig_usr(int signo)
{

#ifdef PKG_MALLOC
	int memlog;
#endif

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
#ifdef SIG_DEBUG /* signal unsafe stuff follows */
					LM_INFO("signal %d received\n", signo);
#endif
				break;
			case SIGINT:
			case SIGTERM:
#ifdef SIG_DEBUG /* signal unsafe stuff follows */
					LM_INFO("signal %d received\n", signo);
					/* print memory stats for non-main too */
					#ifdef PKG_MALLOC
					/* make sure we have current cfg values, but update only
					  the safe part (values not requiring callbacks), to
					  account for processes that might not have registered
					  config support */
					cfg_update_no_cbs();
					memlog=cfg_get(core, core_cfg, memlog);
					if (memlog <= cfg_get(core, core_cfg, debug)){
						if (cfg_get(core, core_cfg, mem_summary) & 1) {
							LOG(memlog, "Memory status (pkg):\n");
							pkg_status();
						}
						if (cfg_get(core, core_cfg, mem_summary) & 4) {
							LOG(memlog, "Memory still-in-use summary (pkg):"
									"\n");
							pkg_sums();
						}
					}
					#endif
#endif
					_exit(0);
					break;
			case SIGUSR1:
#ifdef PKG_MALLOC
					cfg_update_no_cbs();
					memlog=cfg_get(core, core_cfg, memlog);
					if (memlog <= cfg_get(core, core_cfg, debug)){
						if (cfg_get(core, core_cfg, mem_summary) & 1) {
							LOG(memlog, "Memory status (pkg):\n");
							pkg_status();
						}
						if (cfg_get(core, core_cfg, mem_summary) & 4) {
							LOG(memlog, "Memory still-in-use summary (pkg):\n");
							pkg_sums();
						}
					}
#endif
					break;
				/* ignored*/
			case SIGUSR2:
			case SIGHUP:
					break;
			case SIGCHLD:
#ifndef 			STOP_JIRIS_CHANGES
#ifdef SIG_DEBUG /* signal unsafe stuff follows */
					LM_DBG("SIGCHLD received: "
						"we do not worry about grand-children\n");
#endif
#else
					_exit(0); /* terminate if one child died */
#endif
					break;
		}
	}
}



/* install the signal handlers, returns 0 on success, -1 on error */
int install_sigs(void)
{
	/* added by jku: add exit handler */
	if (set_sig_h(SIGINT, sig_usr) == SIG_ERR ) {
		ERR("no SIGINT signal handler can be installed\n");
		goto error;
	}
	/* if we debug and write to a pipe, we want to exit nicely too */
	if (set_sig_h(SIGPIPE, sig_usr) == SIG_ERR ) {
		ERR("no SIGINT signal handler can be installed\n");
		goto error;
	}
	if (set_sig_h(SIGUSR1, sig_usr)  == SIG_ERR ) {
		ERR("no SIGUSR1 signal handler can be installed\n");
		goto error;
	}
	if (set_sig_h(SIGCHLD , sig_usr)  == SIG_ERR ) {
		ERR("no SIGCHLD signal handler can be installed\n");
		goto error;
	}
	if (set_sig_h(SIGTERM , sig_usr)  == SIG_ERR ) {
		ERR("no SIGTERM signal handler can be installed\n");
		goto error;
	}
	if (set_sig_h(SIGHUP , sig_usr)  == SIG_ERR ) {
		ERR("no SIGHUP signal handler can be installed\n");
		goto error;
	}
	if (set_sig_h(SIGUSR2 , sig_usr)  == SIG_ERR ) {
		ERR("no SIGUSR2 signal handler can be installed\n");
		goto error;
	}
	return 0;
error:
	return -1;
}

/* returns -1 on error, 0 on success
 * sets proto */
int parse_proto(unsigned char* s, long len, int* proto)
{
#define PROTO2UINT3(a, b, c) ((	(((unsigned int)(a))<<16)+ \
								(((unsigned int)(b))<<8)+  \
								((unsigned int)(c)) ) | 0x20202020)
#define PROTO2UINT4(a, b ,c ,d) ((	(((unsigned int)(a))<<24)+ \
									(((unsigned int)(b))<<16)+ \
									(((unsigned int)(c))<< 8)+ \
									(((unsigned int)(d))) \
								  )| 0x20202020 )
	unsigned int i;
	if (likely(len==3)){
		i=PROTO2UINT3(s[0], s[1], s[2]);
		switch(i){
			case PROTO2UINT3('u', 'd', 'p'):
				*proto=PROTO_UDP;
				break;
#ifdef USE_TCP
			case PROTO2UINT3('t', 'c', 'p'):
				*proto=PROTO_TCP;
				break;
#ifdef USE_TLS
			case PROTO2UINT3('t', 'l', 's'):
				*proto=PROTO_TLS;
				break;
#endif
#endif
			default:
				return -1;
		}
	}
#ifdef USE_SCTP
	else if (likely(len==4)){
		i=PROTO2UINT4(s[0], s[1], s[2], s[3]);
		if (i==PROTO2UINT4('s', 'c', 't', 'p'))
			*proto=PROTO_SCTP;
		else
			return -1;
	}
#endif /* USE_SCTP */
	else
	/* Deliberately leaving out PROTO_WS and PROTO_WSS as these are just
	   upgraded TCP/TLS connections. */
		return -1;
	return 0;
}



static struct name_lst* mk_name_lst_elem(char* name, int name_len, int flags)
{
	struct name_lst* l;
	
	l=pkg_malloc(sizeof(struct name_lst)+name_len+1/* 0 */);
	if (l){
		l->name=((char*)l)+sizeof(struct name_lst);
		memcpy(l->name, name, name_len);
		l->name[name_len]=0;
		l->flags=flags;
		l->next=0;
	}
	return l;
}



/* free a name_lst list with elements allocated with mk_name_lst_elem
 * (single block both for the structure and for the name) */
static void free_name_lst(struct name_lst* lst)
{
	struct name_lst* l;
	
	while(lst){
		l=lst;
		lst=lst->next;
		pkg_free(l);
	}
}



/* parse h and returns a name lst (flags are set to SI_IS_MHOMED if
 * h contains more then one name or contains a name surrounded by '(' ')' )
 * valid formats:    "hostname"
 *                   "(hostname, hostname1, hostname2)"
 *                   "(hostname hostname1 hostname2)"
 *                   "(hostname)"
 */
static struct name_lst* parse_name_lst(char* h, int h_len)
{
	char* last;
	char* p;
	struct name_lst* n_lst;
	struct name_lst* l;
	struct name_lst** tail;
	int flags;
	
	n_lst=0;
	tail=&n_lst;
	last=h+h_len-1;
	flags=0;
	/* eat whitespace */
	for(; h<=last && ((*h==' ') || (*h=='\t')); h++);
	for(; last>h && ((*last==' ') || (*last=='\t')); last--);
	/* catch empty strings and invalid lens */
	if (h>last) goto error;
	
	if (*h=='('){
		/* list mode */
		if (*last!=')' || ((h+1)>(last-1)))
			goto error;
		h++;
		last--;
		flags=SI_IS_MHOMED;
		for(p=h; p<=last; p++)
			switch (*p){
				case ',':
				case ';':
				case ' ':
				case '\t':
					if ((int)(p-h)>0){
						l=mk_name_lst_elem(h, (int)(p-h), flags);
						if (l==0) 
							goto error;
						*tail=l;
						tail=&l->next;
					}
					h=p+1;
					break;
			}
	}else{
		/* single addr. mode */
		flags=0;
		p=last+1;
	}
	if ((int)(p-h)>0){
		l=mk_name_lst_elem(h, (int)(p-h), flags);
		if (l==0) 
			goto error;
		*tail=l;
		tail=&l->next;
	}
	return n_lst;
error:
	if (n_lst) free_name_lst(n_lst);
	return 0;
}



/*
 * parses [proto:]host[:port]  or
 *  [proto:](host_1, host_2, ... host_n)[:port]
 * where proto= udp|tcp|tls
 * returns  fills proto, port, host and returns list of addresses on success
 * (pkg malloc'ed) and 0 on failure
 */
/** get protocol host and port from a string representation.
 * parses [proto:]host[:port]  or
 *  [proto:](host_1, host_2, ... host_n)[:port]
 * where proto= udp|tcp|tls|sctp
 * @param s  - string (like above)
 * @param host - will be filled with the host part
 *               Note: for multi-homing it wil contain all the addresses
 *               (e.g.: "sctp:(1.2.3.4, 5.6.7.8)" => host="(1.2.3.4, 5.6.7.8)")
 * @param hlen - will be filled with the length of the host part.
 * @param port - will be filled with the port if present or 0 if it's not.
 * @param proto - will be filled with the protocol if present or PROTO_NONE
 *                if it's not.
 * @return  fills proto, port, host and returns 0 on success and -1 on failure.
 */
int parse_phostport(char* s, char** host, int* hlen,
								 int* port, int* proto)
{
	char* first; /* first ':' occurrence */
	char* second; /* second ':' occurrence */
	char* p;
	int bracket;
	char* tmp;

	first=second=0;
	bracket=0;

	/* find the first 2 ':', ignoring possible ipv6 addresses
	 * (substrings between [])
	 */
	for(p=s; *p; p++){
		switch(*p){
			case '[':
				bracket++;
				if (bracket>1) goto error_brackets;
				break;
			case ']':
				bracket--;
				if (bracket<0) goto error_brackets;
				break;
			case ':':
				if (bracket==0){
					if (first==0) first=p;
					else if( second==0) second=p;
					else goto error_colons;
				}
				break;
		}
	}
	if (p==s) return -1;
	if (*(p-1)==':') goto error_colons;

	if (first==0){ /* no ':' => only host */
		*host=s;
		*hlen=(int)(p-s);
		*port=0;
		*proto=0;
		goto end;
	}
	if (second){ /* 2 ':' found => check if valid */
		if (parse_proto((unsigned char*)s, first-s, proto)<0) goto error_proto;
		*port=strtol(second+1, &tmp, 10);
		if ((tmp==0)||(*tmp)||(tmp==second+1)) goto error_port;
		*host=first+1;
		*hlen=(int)(second-*host);
		goto end;
	}
	/* only 1 ':' found => it's either proto:host or host:port */
	*port=strtol(first+1, &tmp, 10);
	if ((tmp==0)||(*tmp)||(tmp==first+1)){
		/* invalid port => it's proto:host */
		if (parse_proto((unsigned char*)s, first-s, proto)<0) goto error_proto;
		*port=0;
		*host=first+1;
		*hlen=(int)(p-*host);
	}else{
		/* valid port => its host:port */
		*proto=0;
		*host=s;
		*hlen=(int)(first-*host);
	}
end:
	return 0;
error_brackets:
	LM_ERR("too many brackets in %s\n", s);
	return -1;
error_colons:
	LM_ERR("too many colons in %s\n", s);
	return -1;
error_proto:
	LM_ERR("bad protocol in %s\n", s);
	return -1;
error_port:
	LM_ERR("bad port number in %s\n", s);
	return -1;
}



/** get protocol host, port and MH addresses list from a string representation.
 * parses [proto:]host[:port]  or
 *  [proto:](host_1, host_2, ... host_n)[:port]
 * where proto= udp|tcp|tls|sctp
 * @param s  - string (like above)
 * @param host - will be filled with the host part
 *               Note: for multi-homing it wil contain all the addresses
 *               (e.g.: "sctp:(1.2.3.4, 5.6.7.8)" => host="(1.2.3.4, 5.6.7.8)")
 * @param hlen - will be filled with the length of the host part.
 * @param port - will be filled with the port if present or 0 if it's not.
 * @param proto - will be filled with the protocol if present or PROTO_NONE
 *                if it's not.
 * @return  fills proto, port, host and returns list of addresses on success
 * (pkg malloc'ed) and 0 on failure
 */
static struct name_lst* parse_phostport_mh(char* s, char** host, int* hlen,
								 int* port, int* proto)
{
	if (parse_phostport(s, host, hlen, port, proto)==0)
		return parse_name_lst(*host, *hlen);
	return 0;
}



/** Update \c cfg_file variable to contain full pathname. The function updates
 * the value of \c cfg_file global variable to contain full absolute pathname
 * to the main configuration file of SER. The function uses CFG_FILE macro to
 * determine the default path to the configuration file if the user did not
 * specify one using the command line option. If \c cfg_file contains an
 * absolute pathname then it is used unmodified, if it contains a relative
 * pathanme than the value returned by \c getcwd function will be added at the
 * beginning. This function must be run before SER changes its current working
 * directory to / (in daemon mode).
 * @return Zero on success, negative number
 * on error.
 */
int fix_cfg_file(void)
{
	char* res = NULL;
	size_t max_len, cwd_len, cfg_len;
	
	if (cfg_file == NULL) cfg_file = CFG_FILE;
	if (cfg_file[0] == '/') return 0;
	if (cfg_file[0] == '-' && strlen(cfg_file)==1) return 0;
	
	/* cfg_file contains a relative pathname, get the current
	 * working directory and add it at the beginning
	 */
	cfg_len = strlen(cfg_file);
	
	max_len = pathmax();
	if ((res = malloc(max_len)) == NULL) goto error;
	
	if (getcwd(res, max_len) == NULL) goto error;
	cwd_len = strlen(res);
	
	/* Make sure that the buffer is big enough */
	if (cwd_len + 1 + cfg_len >= max_len) goto error;
	
	res[cwd_len] = '/';
	memcpy(res + cwd_len + 1, cfg_file, cfg_len);
	
	res[cwd_len + 1 + cfg_len] = '\0'; /* Add terminating zero */
	cfg_file = res;
	return 0;
	
 error:
	fprintf(stderr, "ERROR: Unable to fix cfg_file to contain full pathname\n");
	if (res) free(res);
	return -1;
}


/* main loop */
int main_loop(void)
{
	int  i;
	pid_t pid;
	struct socket_info* si;
	char si_desc[MAX_PT_DESC];
#ifdef EXTRA_DEBUG
	int r;
#endif
	int nrprocs;
	int woneinit;

	/* one "main" process and n children handling i/o */
	if (dont_fork){
#ifdef STATS
		setstats( 0 );
#endif
		if (udp_listen==0){
			LM_ERR("no fork mode requires at least one"
					" udp listen address, exiting...\n");
			goto error;
		}
		/* only one address, we ignore all the others */
		if (udp_init(udp_listen)==-1) goto error;
		bind_address=udp_listen;
		if (bind_address->address.af==AF_INET) {
			sendipv4=bind_address;
#ifdef USE_RAW_SOCKS
		/* always try to have a raw socket opened if we are using ipv4 */
		raw_udp4_send_sock = raw_socket(IPPROTO_RAW, 0, 0, 1);
		if (raw_udp4_send_sock < 0) {
			if ( default_core_cfg.udp4_raw > 0) {
				/* force use raw socket failed */
				ERR("could not initialize raw udp send socket (ipv4):"
						" %s (%d)\n", strerror(errno), errno);
				if (errno == EPERM)
					ERR("could not initialize raw socket on startup"
						" due to inadequate permissions, please"
						" restart as root or with CAP_NET_RAW\n");
				goto error;
			}
			default_core_cfg.udp4_raw = 0; /* disabled */
		} else {
			register_fds(1);
			if (default_core_cfg.udp4_raw < 0) {
				/* auto-detect => use it */
				default_core_cfg.udp4_raw = 1; /* enabled */
				LM_DBG("raw socket possible => turning it on\n");
			}
			if (default_core_cfg.udp4_raw_ttl < 0) {
				/* auto-detect */
				default_core_cfg.udp4_raw_ttl = sock_get_ttl(sendipv4->socket);
				if (default_core_cfg.udp4_raw_ttl < 0)
					/* error, use some default value */
					default_core_cfg.udp4_raw_ttl = 63;
			}
		}
#else
		default_core_cfg.udp4_raw = 0;
#endif /* USE_RAW_SOCKS */
		} else
			sendipv6=bind_address;
		if (udp_listen->next){
			LM_WARN("using only the first listen address (no fork)\n");
		}

		/* delay cfg_shmize to the last moment (it must be called _before_
		   forking). Changes to default cfgs after this point will be
		   ignored.
		*/
		if (cfg_shmize() < 0) {
			LM_CRIT("could not initialize shared configuration\n");
			goto error;
		}

		/* Register the children that will keep updating their
		 * local configuration */
		cfg_register_child(
				1   /* main = udp listener */
				+ 1 /* timer */
#ifdef USE_SLOW_TIMER
				+ 1 /* slow timer */
#endif
			);
		if (do_suid()==-1) goto error; /* try to drop privileges */
		/* process_no now initialized to zero -- increase from now on
		   as new processes are forked (while skipping 0 reserved for main
		*/

		/* Temporary set the local configuration of the main process
		 * to make the group instances available in PROC_INIT.
		 */
		cfg_main_set_local();

		/* init log prefix format */
		log_prefix_init();

		/* init childs with rank==PROC_INIT before forking any process,
		 * this is a place for delayed (after mod_init) initializations
		 * (e.g. shared vars that depend on the total number of processes
		 * that is known only after all mod_inits have been executed )
		 * WARNING: the same init_child will be called latter, a second time
		 * for the "main" process with rank PROC_MAIN (make sure things are
		 * not initialized twice)*/
		if (init_child(PROC_INIT) < 0) {
			LM_ERR("init_child(PROC_INT) -- exiting\n");
			cfg_main_reset_local();
			goto error;
		}
		cfg_main_reset_local();
		if (counters_prefork_init(get_max_procs()) == -1) goto error;

#ifdef USE_SLOW_TIMER
		/* we need another process to act as the "slow" timer*/
				pid = fork_process(PROC_TIMER, "slow timer", 0);
				if (pid<0){
					LM_CRIT("Cannot fork\n");
					goto error;
				}
				if (pid==0){
					/* child */
					/* timer!*/
					if (real_time&2)
						set_rt_prio(rt_timer2_prio, rt_timer2_policy);

					if (arm_slow_timer()<0) goto error;
					slow_timer_main();
				}else{
					slow_timer_pid=pid;
				}
#endif
				/* we need another process to act as the "main" timer*/
				pid = fork_process(PROC_TIMER, "timer", 0);
				if (pid<0){
					LM_CRIT("Cannot fork\n");
					goto error;
				}
				if (pid==0){
					/* child */
					/* timer!*/
					if (real_time&1)
						set_rt_prio(rt_timer1_prio, rt_timer1_policy);
					if (arm_timer()<0) goto error;
					timer_main();
				}else{
				}

		if(sr_wtimer_start()<0) {
			LM_CRIT("Cannot start wtimer\n");
			goto error;
		}
		/* main process, receive loop */
		process_no=0; /*main process number*/
		pt[process_no].pid=getpid();
		snprintf(pt[process_no].desc, MAX_PT_DESC,
			"stand-alone receiver @ %s:%s",
			 bind_address->name.s, bind_address->port_no_str.s );

		/* call it also w/ PROC_MAIN to make sure modules that init things 
		 * only in PROC_MAIN get a chance to run */
		if (init_child(PROC_MAIN) < 0) {
			LM_ERR("init_child(PROC_MAIN) -- exiting\n");
			goto error;
		}

		/* We will call child_init even if we
		 * do not fork - and it will be called with rank 1 because
		 * in fact we behave like a child, not like main process
		 */

		if (init_child(PROC_SIPINIT) < 0) {
			LM_ERR("init_child failed\n");
			goto error;
		}
		return udp_rcv_loop();
	}else{ /* fork: */

		/* Register the children that will keep updating their
		 * local configuration. (udp/tcp/sctp listeneres
		 * will be added later.) */
		cfg_register_child(
				1   /* timer */
#ifdef USE_SLOW_TIMER
				+ 1 /* slow timer */
#endif
			);

		for(si=udp_listen;si;si=si->next){
			/* create the listening socket (for each address)*/
			/* udp */
			if (udp_init(si)==-1) goto error;
			/* get first ipv4/ipv6 socket*/
			if ((si->address.af==AF_INET)&&
					((sendipv4==0)||(sendipv4->flags&(SI_IS_LO|SI_IS_MCAST))))
				sendipv4=si;
			if ( ((sendipv6==0)||(sendipv6->flags&(SI_IS_LO|SI_IS_MCAST))) &&
					(si->address.af==AF_INET6))
				sendipv6=si;
			/* children_no per each socket */
			cfg_register_child((si->workers>0)?si->workers:children_no);
		}
#ifdef USE_RAW_SOCKS
		/* always try to have a raw socket opened if we are using ipv4 */
		if (sendipv4) {
			raw_udp4_send_sock = raw_socket(IPPROTO_RAW, 0, 0, 1);
			if (raw_udp4_send_sock < 0) {
				if ( default_core_cfg.udp4_raw > 0) {
						/* force use raw socket failed */
						ERR("could not initialize raw udp send socket (ipv4):"
								" %s (%d)\n", strerror(errno), errno);
						if (errno == EPERM)
							ERR("could not initialize raw socket on startup"
								" due to inadequate permissions, please"
								" restart as root or with CAP_NET_RAW\n");
						goto error;
					}
					default_core_cfg.udp4_raw = 0; /* disabled */
			} else {
				register_fds(1);
				if (default_core_cfg.udp4_raw < 0) {
					/* auto-detect => use it */
					default_core_cfg.udp4_raw = 1; /* enabled */
					LM_DBG("raw socket possible => turning it on\n");
				}
				if (default_core_cfg.udp4_raw_ttl < 0) {
					/* auto-detect */
					default_core_cfg.udp4_raw_ttl =
						sock_get_ttl(sendipv4->socket);
					if (default_core_cfg.udp4_raw_ttl < 0)
						/* error, use some default value */
						default_core_cfg.udp4_raw_ttl = 63;
				}
			}
		}
#else
		default_core_cfg.udp4_raw = 0;
#endif /* USE_RAW_SOCKS */
#ifdef USE_SCTP
		if (!sctp_disable){
			for(si=sctp_listen; si; si=si->next){
				if (sctp_core_init_sock(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET) &&
						((sendipv4_sctp==0) ||
							(sendipv4_sctp->flags&(SI_IS_LO|SI_IS_MCAST))))
					sendipv4_sctp=si;
				if( ((sendipv6_sctp==0) || 
							(sendipv6_sctp->flags&(SI_IS_LO|SI_IS_MCAST))) &&
						(si->address.af==AF_INET6))
					sendipv6_sctp=si;
				/* sctp_children_no per each socket */
				cfg_register_child((si->workers>0)?si->workers:sctp_children_no);
			}
		}
#endif /* USE_SCTP */
#ifdef USE_TCP
		if (!tcp_disable){
			for(si=tcp_listen; si; si=si->next){
				/* same thing for tcp */
				if (tcp_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tcp==0) ||
							(sendipv4_tcp->flags&(SI_IS_LO|SI_IS_MCAST))))
					sendipv4_tcp=si;
				if( ((sendipv6_tcp==0) ||
							(sendipv6_tcp->flags&(SI_IS_LO|SI_IS_MCAST))) &&
						(si->address.af==AF_INET6))
					sendipv6_tcp=si;
			}
			/* the number of sockets does not matter */
			cfg_register_child(tcp_children_no + 1 /* tcp main */);
		}
#ifdef USE_TLS
		if (!tls_disable && tls_has_init_si()){
			for(si=tls_listen; si; si=si->next){
				/* same as for tcp*/
				if (tls_init(si)==-1)  goto error;
				/* get first ipv4/ipv6 socket*/
				if ((si->address.af==AF_INET)&&
						((sendipv4_tls==0) ||
							(sendipv4_tls->flags&(SI_IS_LO|SI_IS_MCAST))))
					sendipv4_tls=si;
				if( ((sendipv6_tls==0) ||
							(sendipv6_tls->flags&(SI_IS_LO|SI_IS_MCAST))) &&
						(si->address.af==AF_INET6))
					sendipv6_tls=si;
			}
		}
#endif /* USE_TLS */
#endif /* USE_TCP */

			/* all processes should have access to all the sockets (for 
			 * sending) so we open all first*/
		if (do_suid()==-1) goto error; /* try to drop privileges */

		/* delay cfg_shmize to the last moment (it must be called _before_
		   forking). Changes to default cfgs after this point will be
		   ignored (cfg_shmize() will copy the default cfgs into shmem).
		*/
		if (cfg_shmize() < 0) {
			LM_CRIT("could not initialize shared configuration\n");
			goto error;
		}

		/* Temporary set the local configuration of the main process
		 * to make the group instances available in PROC_INIT.
		 */
		cfg_main_set_local();

		/* init log prefix format */
		log_prefix_init();

		/* init childs with rank==PROC_INIT before forking any process,
		 * this is a place for delayed (after mod_init) initializations
		 * (e.g. shared vars that depend on the total number of processes
		 * that is known only after all mod_inits have been executed )
		 * WARNING: the same init_child will be called latter, a second time
		 * for the "main" process with rank PROC_MAIN (make sure things are
		 * not initialized twice)*/
		if (init_child(PROC_INIT) < 0) {
			LM_ERR("error in init_child(PROC_INT) -- exiting\n");
			cfg_main_reset_local();
			goto error;
		}
		cfg_main_reset_local();
		if (counters_prefork_init(get_max_procs()) == -1) goto error;


		woneinit = 0;
		/* udp processes */
		for(si=udp_listen; si; si=si->next){
			nrprocs = (si->workers>0)?si->workers:children_no;
			for(i=0;i<nrprocs;i++){
				if(si->address.af==AF_INET6) {
					if(si->useinfo.name.s)
						snprintf(si_desc, MAX_PT_DESC, "udp receiver child=%d "
							"sock=[%s]:%s (%s:%s)",
							i, si->name.s, si->port_no_str.s,
							si->useinfo.name.s, si->useinfo.port_no_str.s);
					else
						snprintf(si_desc, MAX_PT_DESC, "udp receiver child=%d "
							"sock=[%s]:%s",
							i, si->name.s, si->port_no_str.s);
				} else {
					if(si->useinfo.name.s)
						snprintf(si_desc, MAX_PT_DESC, "udp receiver child=%d "
							"sock=%s:%s (%s:%s)",
							i, si->name.s, si->port_no_str.s,
							si->useinfo.name.s, si->useinfo.port_no_str.s);
					else
						snprintf(si_desc, MAX_PT_DESC, "udp receiver child=%d "
							"sock=%s:%s",
							i, si->name.s, si->port_no_str.s);
				}
				child_rank++;
				pid = fork_process(child_rank, si_desc, 1);
				if (pid<0){
					LM_CRIT("Cannot fork\n");
					goto error;
				}else if (pid==0){
					/* child */
					bind_address=si; /* shortcut */
#ifdef STATS
					setstats( i+r*children_no );
#endif
					if(woneinit==0) {
						if(run_child_one_init_route()<0)
							goto error;
					}
					return udp_rcv_loop();
				}
				woneinit = 1;
			}
			/*parent*/
			/*close(udp_sock)*/; /*if it's closed=>sendto invalid fd errors?*/
		}
#ifdef USE_SCTP
		/* sctp processes */
		if (!sctp_disable){
			for(si=sctp_listen; si; si=si->next){
				nrprocs = (si->workers>0)?si->workers:sctp_children_no;
				for(i=0;i<nrprocs;i++){
					if(si->address.af==AF_INET6) {
						snprintf(si_desc, MAX_PT_DESC, "sctp receiver child=%d "
								"sock=[%s]:%s",
								i, si->name.s, si->port_no_str.s);
					} else {
						snprintf(si_desc, MAX_PT_DESC, "sctp receiver child=%d "
								"sock=%s:%s",
								i, si->name.s, si->port_no_str.s);
					}
					child_rank++;
					pid = fork_process(child_rank, si_desc, 1);
					if (pid<0){
						LM_CRIT("Cannot fork\n");
						goto error;
					}else if (pid==0){
						/* child */
						bind_address=si; /* shortcut */
#ifdef STATS
						setstats( i+r*children_no );
#endif
						return sctp_core_rcv_loop();
					}
				}
			/*parent*/
			/*close(sctp_sock)*/; /*if closed=>sendto invalid fd errors?*/
			}
		}
#endif /* USE_SCTP */

		/*this is the main process*/
		bind_address=0;	/* main proc -> it shouldn't send anything, */

#ifdef USE_SLOW_TIMER
		/* fork again for the "slow" timer process*/
		pid = fork_process(PROC_TIMER, "slow timer", 1);
		if (pid<0){
			LM_CRIT("cannot fork \"slow\" timer process\n");
			goto error;
		}else if (pid==0){
			/* child */
			if (real_time&2)
				set_rt_prio(rt_timer2_prio, rt_timer2_policy);
			if (arm_slow_timer()<0) goto error;
			slow_timer_main();
		}else{
			slow_timer_pid=pid;
		}
#endif /* USE_SLOW_TIMER */

		/* fork again for the "main" timer process*/
		pid = fork_process(PROC_TIMER, "timer", 1);
		if (pid<0){
			LM_CRIT("cannot fork timer process\n");
			goto error;
		}else if (pid==0){
			/* child */
			if (real_time&1)
				set_rt_prio(rt_timer1_prio, rt_timer1_policy);
			if (arm_timer()<0) goto error;
			timer_main();
		}
		if(sr_wtimer_start()<0) {
			LM_CRIT("Cannot start wtimer\n");
			goto error;
		}

	/* init childs with rank==MAIN before starting tcp main (in case they want
	 * to fork  a tcp capable process, the corresponding tcp. comm. fds in
	 * pt[] must be set before calling tcp_main_loop()) */
		if (init_child(PROC_MAIN) < 0) {
			LM_ERR("error in init_child\n");
			goto error;
		}

#ifdef USE_TCP
		if (!tcp_disable){
				/* start tcp  & tls receivers */
			if (tcp_init_children()<0) goto error;
				/* start tcp+tls master proc */
			pid = fork_process(PROC_TCP_MAIN, "tcp main process", 0);
			if (pid<0){
				LM_CRIT("cannot fork tcp main process: %s\n", strerror(errno));
				goto error;
			}else if (pid==0){
				/* child */
				tcp_main_loop();
			}else{
				tcp_main_pid=pid;
				unix_tcp_sock=-1;
			}
		}
#endif
		/* main */
		strncpy(pt[0].desc, "main process - attendant", MAX_PT_DESC );
#ifdef USE_TCP
		close_extra_socks(PROC_ATTENDANT, get_proc_no());
		if(!tcp_disable){
			/* main's tcp sockets are disabled by default from init_pt() */
			unix_tcp_sock=-1;
		}
#endif
		/* init cfg, but without per child callbacks support */
		cfg_child_no_cb_init();
		cfg_ok=1;

#ifdef EXTRA_DEBUG
		for (r=0; r<*process_count; r++){
			fprintf(stderr, "% 3d   % 5d - %s\n", r, pt[r].pid, pt[r].desc);
		}
#endif
		LM_DBG("Expect maximum %d  open fds\n", get_max_open_fds());
		/* in daemonize mode send the exit code back to the parent process */
		if (!dont_daemonize) {
			if (daemon_status_send(0) < 0) {
				ERR("error sending daemon status: %s [%d]\n",
						strerror(errno), errno);
				goto error;
			}
		}
		for(;;){
			handle_sigs();
			pause();
			cfg_update();
		}
	
	}

	/*return 0; */
error:
				 /* if we are here, we are the "main process",
				  any forked children should exit with exit(-1) and not
				  ever use return */
	return -1;

}

/*
 * Calculate number of processes, this does not
 * include processes created by modules
 */
static int calc_proc_no(void)
{
	int udp_listeners;
	struct socket_info* si;
#ifdef USE_TCP
	int tcp_listeners;
	int tcp_e_listeners;
#endif
#ifdef USE_SCTP
	int sctp_listeners;
#endif

	for (si=udp_listen, udp_listeners=0; si; si=si->next)
		udp_listeners += (si->workers>0)?si->workers:children_no;
#ifdef USE_TCP
	for (si=tcp_listen, tcp_listeners=0, tcp_e_listeners=0; si; si=si->next) {
		if(si->workers>0)
			tcp_listeners += si->workers;
		else
			 tcp_e_listeners = tcp_cfg_children_no;
	}
	tcp_listeners += tcp_e_listeners;
#ifdef USE_TLS
	tcp_e_listeners = 0;
	for (si=tls_listen, tcp_e_listeners=0; si; si=si->next) {
		if(si->workers>0)
			tcp_listeners += si->workers;
		else {
			if(tcp_listeners==0)
				tcp_e_listeners = tcp_cfg_children_no;
		}
	}
	tcp_listeners += tcp_e_listeners;
#endif
	tcp_children_no = tcp_listeners;
#endif
#ifdef USE_SCTP
	for (si=sctp_listen, sctp_listeners=0; si; si=si->next)
		sctp_listeners += (si->workers>0)?si->workers:sctp_children_no;
#endif
	return
		     /* receivers and attendant */
		(dont_fork ? 1 : udp_listeners + 1)
		     /* timer process */
		+ 1 /* always, we need it in most cases, and we can't tell here
		       & now if we don't need it */
#ifdef USE_SLOW_TIMER
		+ 1 /* slow timer process */
#endif
#ifdef USE_TCP
		+((!tcp_disable)?( 1/* tcp main */ + tcp_listeners ):0)
#endif
#ifdef USE_SCTP
		+((!sctp_disable)?sctp_listeners:0)
#endif
		;
}

int main(int argc, char** argv)
{

	FILE* cfg_stream;
	int c,r;
	char *tmp;
	int tmp_len;
	int port;
	int proto;
	char *options;
	int ret;
	unsigned int seed;
	int rfd;
	int debug_save, debug_flag;
	int dont_fork_cnt;
	struct name_lst* n_lst;
	char *p;
	struct stat st = {0};

	/*init*/
	time(&up_since);
	creator_pid = getpid();
	ret=-1;
	my_argc=argc; my_argv=argv;
	debug_flag=0;
	dont_fork_cnt=0;

	sr_cfgenv_init();
	daemon_status_init();

	dprint_init_colors();

	/* command line options */
	options=  ":f:cm:M:dVIhEeb:l:L:n:vKrRDTN:W:w:t:u:g:P:G:SQ:O:a:A:x:X:Y:"
#ifdef STATS
		"s:"
#endif
	;
	/* Handle special command line arguments, that must be treated before
	 * intializing the various subsystem or before parsing other arguments:
	 *  - get the startup debug and log_stderr values
	 *  - look if pkg mem size is overriden on the command line (-M) and get
	 *    the new value here (before intializing pkg_mem).
	 *  - look if there is a -h, e.g. -f -h construction won't be caught
	 *    later
	 */
	opterr = 0;
	while((c=getopt(argc,argv,options))!=-1) {
		switch(c) {
			case 'd':
					debug_flag = 1;
					default_core_cfg.debug++;
					break;
			case 'E':
					log_stderr=1;
					break;
			case 'e':
					log_color=1;
					break;
			case 'M':
					pkg_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad private mem size number: -M %s\n",
											optarg);
						goto error;
					};
					break;
			case 'x':
					sr_memmng_shm = optarg;
					break;
			case 'X':
					sr_memmng_pkg = optarg;
					break;
			default:
					if (c == 'h' || (optarg && strcmp(optarg, "-h") == 0)) {
						printf("version: %s\n", full_version);
						printf("%s",help_msg);
						exit(0);
					}
					break;
		}
	}

	if(sr_memmng_pkg==NULL) {
		if(sr_memmng_shm!=NULL) {
			sr_memmng_pkg = sr_memmng_shm;
		} else {
			sr_memmng_pkg = SR_MEMMNG_DEFAULT;
		}
	}
	if(sr_memmng_shm==NULL) {
		sr_memmng_shm = SR_MEMMNG_DEFAULT;
	}
	shm_set_mname(sr_memmng_shm);
	if (pkg_mem_size == 0) {
		pkg_mem_size = PKG_MEM_POOL_SIZE;
	}

	/*init pkg mallocs (before parsing cfg or the rest of the cmd line !)*/
	if (pkg_mem_size)
		LM_INFO("private (per process) memory: %ld bytes\n", pkg_mem_size );
	if (pkg_init_manager(sr_memmng_pkg)<0)
		goto error;

#ifdef DBG_MSG_QA
	fprintf(stderr, "WARNING: ser startup: "
		"DBG_MSG_QA enabled, ser may exit abruptly\n");
#endif

	/* init counters / stats */
	if (init_counters() == -1)
		goto error;
#ifdef USE_TCP
	init_tcp_options(); /* set the defaults before the config */
#endif

	/* process command line (cfg. file path etc) */
	optind = 1;  /* reset getopt */
	/* switches required before script processing */
	while((c=getopt(argc,argv,options))!=-1) {
		switch(c) {
			case 'M':
			case 'x':
			case 'X':
					/* ignore, they were parsed immediately after startup */
					break;
			case 'f':
					cfg_file=optarg;
					break;
			case 'c':
					config_check=1;
					log_stderr=1; /* force stderr logging */
					break;
			case 'L':
					mods_dir = optarg;
					mods_dir_cmd = 1;
					break;
			case 'm':
					shm_mem_size=strtol(optarg, &tmp, 10) * 1024 * 1024;
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad shmem size number: -m %s\n",
										optarg);
						goto error;
					};
					LM_INFO("shared memory: %ld bytes\n", shm_mem_size );
					break;
			case 'd':
					/* ignore it, was parsed immediately after startup */
					break;
			case 'v':
			case 'V':
					printf("version: %s\n", full_version);
					printf("flags: %s\n", ver_flags );
					print_ct_constants();
					printf("id: %s\n", ver_id);
					if(strlen(ver_compiled_time)>0)
						printf("compiled on %s with %s\n",
							ver_compiled_time, ver_compiler );
					else
						printf("compiled with %s\n",
							ver_compiler );

					exit(0);
					break;
			case 'I':
					print_internals();
					exit(0);
					break;
			case 'E':
					/* ignore it, was parsed immediately after startup */
					break;
			case 'e':
					/* ignore it, was parsed immediately after startup */
					break;
			case 'O':
					scr_opt_lev=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad optimization level: -O %s\n",
										optarg);
						goto error;
					};
					break;
			case 'u':
					/* user needed for possible shm. pre-init */
					user=optarg;
					break;
			case 'A':
					p = strchr(optarg, '=');
					if(p) {
						*p = '\0';
					}
					pp_define_set_type(0);
					if(pp_define(strlen(optarg), optarg)<0) {
						fprintf(stderr, "error at define param: -A %s\n",
								optarg);
						goto error;
					}
					if(p) {
						*p = '=';
						p++;
						if(pp_define_set(strlen(p), p)<0) {
							fprintf(stderr, "error at define value: -A %s\n",
								optarg);
							goto error;
						}
					}
					break;
			case 'b':
			case 'l':
			case 'n':
			case 'K':
			case 'r':
			case 'R':
			case 'D':
			case 'T':
			case 'N':
			case 'W':
			case 'w':
			case 't':
			case 'g':
			case 'P':
			case 'G':
			case 'S':
			case 'Q':
			case 'a':
			case 's':
			case 'Y':
					break;
			case '?':
					if (isprint(optopt)) {
						fprintf(stderr, "Unknown option `-%c'."
										" Use -h for help.\n", optopt);
					} else {
						fprintf(stderr, "Unknown option character `\\x%x'."
										" Use -h for help.\n",
							optopt);
					}
					goto error;
			case ':':
					fprintf(stderr, "Option `-%c' requires an argument."
									" Use -h for help.\n",
						optopt);
					goto error;
			default:
					abort();
		}
	}
	if (shm_mem_size == 0) {
		shm_mem_size = SHM_MEM_POOL_SIZE;
	}

	if (endianness_sanity_check() != 0){
		fprintf(stderr, "BUG: endianness sanity tests failed\n");
		goto error;
	}
	if (init_routes()<0) goto error;
	if (init_nonsip_hooks()<0) goto error;
	if (init_script_cb()<0) goto error;
	if (pv_init_api()<0) goto error;
	if (pv_register_core_vars()!=0) goto error;
	if (init_rpcs()<0) goto error;
	if (register_core_rpcs()!=0) goto error;

	/* Fix the value of cfg_file variable.*/
	if (fix_cfg_file() < 0) goto error;

	/* load config file or die */
	if (cfg_file[0] == '-' && strlen(cfg_file)==1) {
		cfg_stream=stdin;
	} else {
		cfg_stream=fopen (cfg_file, "r");
	}
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
			LM_WARN("could not read from /dev/urandom (%d)\n", errno);
		}
		LM_DBG("read %u from /dev/urandom\n", seed);
			close(rfd);
	}else{
		LM_WARN("could not open /dev/urandom (%d)\n", errno);
	}
	seed+=getpid()+time(0);
	LM_DBG("seeding PRNG with %u\n", seed);
	kam_srand(seed);
	fastrand_seed(kam_rand());
	srandom(kam_rand()+time(0));
	LM_DBG("test random numbers %u %lu %u\n", kam_rand(), random(), fastrand());

	/*register builtin  modules*/
	register_builtin_modules();

	/* init named flags */
	init_named_flags();

	yyin=cfg_stream;
	debug_save = default_core_cfg.debug;
	if ((yyparse()!=0)||(cfg_errors)){
		fprintf(stderr, "ERROR: bad config file (%d errors)\n", cfg_errors);
		if (debug_flag) default_core_cfg.debug = debug_save;
		pp_ifdef_level_check();

		goto error;
	}
	if (cfg_warnings){
		fprintf(stderr, "%d config warnings\n", cfg_warnings);
	}
	if (debug_flag) default_core_cfg.debug = debug_save;
	pp_ifdef_level_check();
	print_rls();

	if(init_dst_set()<0) {
		LM_ERR("failed to initialize destination set structure\n");
		goto error;
	}
	/* options with higher priority than cfg file */
	optind = 1;  /* reset getopt */
	while((c=getopt(argc,argv,options))!=-1) {
		switch(c) {
			case 'f':
			case 'c':
			case 'm':
			case 'M':
			case 'd':
			case 'v':
			case 'V':
			case 'I':
			case 'h':
			case 'O':
			case 'A':
					break;
			case 'E':
					log_stderr=1;	/* use in both getopt switches,
									   takes priority over config */
					break;
			case 'e':
					log_color=1;	/* use in both getopt switches,
									   takes priority over config */
					break;
			case 'b':
					maxbuffer=strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						fprintf(stderr, "bad max buffer size number: -b %s\n",
											optarg);
						goto error;
					}
					break;
			case 'l':
					if ((n_lst=parse_phostport_mh(optarg, &tmp, &tmp_len,
											&port, &proto))==0){
						fprintf(stderr, "bad -l address specifier: %s\n",
										optarg);
						goto error;
					}
					/* add a new addr. to our address list */
					if (add_listen_iface(n_lst->name, n_lst->next,  port,
											proto, n_lst->flags)!=0){
						fprintf(stderr, "failed to add new listen address\n");
						free_name_lst(n_lst);
						goto error;
					}
					free_name_lst(n_lst);
					break;
			case 'n':
					children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad process number: -n %s\n",
									optarg);
						goto error;
					}
					break;
			case 'K':
					check_via=1;
					break;
			case 'r':
					received_dns|=DO_DNS;
					break;
			case 'R':
					received_dns|=DO_REV_DNS;
					break;
			case 'D':
					dont_fork_cnt++;
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
					tcp_cfg_children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad process number: -N %s\n",
									optarg);
						goto error;
					}
				#else
					fprintf(stderr,"WARNING: tcp support not compiled in\n");
				#endif
					break;
			case 'W':
				#ifdef USE_TCP
					tcp_poll_method=get_poll_type(optarg);
					if (tcp_poll_method==POLL_NONE){
						fprintf(stderr, "bad poll method name: -W %s\ntry "
										"one of %s.\n", optarg, poll_support);
						goto error;
					}
				#else
					fprintf(stderr,"WARNING: tcp support not compiled in\n");
				#endif
					break;
			case 'S':
				#ifdef USE_SCTP
					sctp_disable=1;
				#else
					fprintf(stderr,"WARNING: sctp support not compiled in\n");
				#endif
					break;
			case 'Q':
				#ifdef USE_SCTP
					sctp_children_no=strtol(optarg, &tmp, 10);
					if ((tmp==0) ||(*tmp)){
						fprintf(stderr, "bad process number: -O %s\n",
									optarg);
						goto error;
					}
				#else
					fprintf(stderr,"WARNING: sctp support not compiled in\n");
				#endif
					break;
			case 'w':
					working_dir=optarg;
					break;
			case 'Y':
					runtime_dir=optarg;
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
			case 'a':
					if(strcmp(optarg, "on")==0 || strcmp(optarg, "yes")==0)
						sr_auto_aliases = 1;
					else if(strcmp(optarg, "off")==0 || strcmp(optarg, "no")==0)
						sr_auto_aliases = 0;
					else {
						fprintf(stderr,
							"bad auto aliases parameter: %s (valid on, off, yes, no)\n",
							optarg);
						goto error;
					}
					break;
			case 's':
				#ifdef STATS
					stat_file=optarg;
				#endif
					break;
			default:
					break;
		}
	}

	/* reinit if pv buffer size has been set in config */
	if (pv_reinit_buffer()<0)
		goto error;

	/* init lookup for core event routes */
	sr_core_ert_init();

	if (dont_fork_cnt)
		dont_fork = dont_fork_cnt;	/* override by command line */

	if (dont_fork > 0) {
		dont_daemonize = dont_fork == 2;
		dont_fork = dont_fork == 1;
	}
	/* init locks first */
	if (init_lock_ops()!=0)
		goto error;
#ifdef USE_TCP
#ifdef USE_TLS
	if (tcp_disable)
		tls_disable=1; /* if no tcp => no tls */
#endif /* USE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
	if (sctp_disable!=1){
		/* fix it */
		if (sctp_core_check_support()==-1){
			/* check if sctp support is auto, if not warn about disabling it */
			if (sctp_disable!=2){
				fprintf(stderr, "ERROR: " "sctp enabled, but not supported by"
								" the OS\n");
				goto error;
			}
			sctp_disable=1;
		}else{
			/* sctp_disable!=1 and sctp supported => enable sctp */
			sctp_disable=0;
		}
	}
#endif /* USE_SCTP */
	/* initialize the configured proto list */
	init_proto_order();
	/* init the resolver, before fixing the config */
	resolv_init();
	/* fix parameters */
	if (port_no<=0) port_no=SIP_PORT;
#ifdef USE_TLS
	if (tls_port_no<=0) tls_port_no=SIPS_PORT;
#endif


	if (children_no<=0) children_no=CHILD_NO;
#ifdef USE_TCP
	if (!tcp_disable){
		if (tcp_cfg_children_no<=0) tcp_cfg_children_no=children_no;
		tcp_children_no = tcp_cfg_children_no;
	}
#endif
#ifdef USE_SCTP
	if (!sctp_disable){
		if (sctp_children_no<=0) sctp_children_no=children_no;
	}
#endif

	if (working_dir==0) working_dir="/";

	/* get uid/gid */
	if (user){
		if (user2uid(&uid, &gid, user)<0){
			fprintf(stderr, "bad user name/uid number: -u %s\n", user);
			goto error;
		}
		sock_uid = uid;
		sock_gid = gid;
	}
	if (group){
		if (group2gid(&gid, group)<0){
				fprintf(stderr, "bad group name/gid number: -u %s\n", group);
			goto error;
		}
		sock_gid = gid;
	}
	/* create runtime dir if doesn't exist */
	if (stat(runtime_dir, &st) == -1) {
		if(mkdir(runtime_dir, 0700) == -1) {
			LM_ERR("failed to create runtime dir %s\n", runtime_dir);
			fprintf(stderr,  "failed to create runtime dir %s\n", runtime_dir);
			goto error;
		}
		if(sock_uid!=-1 || sock_gid!=-1) {
			if(chown(runtime_dir, sock_uid, sock_gid) == -1) {
				LM_ERR("failed to change owner of runtime dir %s\n", runtime_dir);
				fprintf(stderr,  "failed to change owner of runtime dir %s\n", runtime_dir);
				goto error;
			}
		}
	}
	if (fix_all_socket_lists()!=0){
		fprintf(stderr,  "failed to initialize list addresses\n");
		goto error;
	}
	if (default_core_cfg.dns_try_ipv6 && !(socket_types & SOCKET_T_IPV6)){
		/* if we are not listening on any ipv6 address => no point
		 * to try to resovle ipv6 addresses */
		default_core_cfg.dns_try_ipv6=0;
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
				(udp_listen->next)?"and more than one listen address found "
				"(will use only the first one)":""
				):"and no udp listen address found" );
	}
	if (config_check){
		fprintf(stderr, "config file ok, exiting...\n");
		return 0;
	}


	/*init shm mallocs
	 *  this must be here
	 *     -to allow setting shm mem size from the command line
	 *       => if shm_mem should be settable from the cfg file move
	 *       everything after
	 *     -it must be also before init_timer and init_tcp
	 *     -it must be after we know uid (so that in the SYSV sems case,
	 *        the sems will have the correct euid)
	 *  Note: shm can now be initialized when parsing the config script, that's
	 *  why checking for a prior initialization is needed.
	 * --andrei */
#ifdef SHM_MEM
	if (!shm_initialized() && init_shm()<0)
		goto error;
#endif /* SHM_MEM */
	pkg_print_manager();
	shm_print_manager();
	if (init_atomic_ops()==-1)
		goto error;
	if (init_basex() != 0){
		LM_CRIT("could not initialize base* framework\n");
		goto error;
	}
	if (sr_cfg_init() < 0) {
		LM_CRIT("could not initialize configuration framework\n");
		goto error;
	}
	/* declare the core cfg before the module configs */
	if (cfg_declare("core", core_cfg_def, &default_core_cfg, cfg_sizeof(core),
			&core_cfg)
	) {
		LM_CRIT("could not declare the core configuration\n");
		goto error;
	}
#ifdef USE_TCP
	if (tcp_register_cfg()){
		LM_CRIT("could not register the tcp configuration\n");
		goto error;
	}
#endif /* USE_TCP */
	/*init timer, before parsing the cfg!*/
	if (init_timer()<0){
		LM_CRIT("could not initialize timer, exiting...\n");
		goto error;
	}
	/* init wtimer */
	if(sr_wtimer_init()<0) {
		LM_CRIT("could not initialize wtimer, exiting...\n");
		goto error;
	}

#ifdef USE_DNS_CACHE
	if (init_dns_cache()<0){
		LM_CRIT("could not initialize the dns cache, exiting...\n");
		goto error;
	}
#ifdef USE_DNS_CACHE_STATS
	/* preinitializing before the nubmer of processes is determined */
	if (init_dns_cache_stats(1)<0){
		LM_CRIT("could not initialize the dns cache measurement\n");
		goto error;
	}
#endif /* USE_DNS_CACHE_STATS */
#endif
#ifdef USE_DST_BLACKLIST
	if (init_dst_blacklist()<0){
		LM_CRIT("could not initialize the dst blacklist, exiting...\n");
		goto error;
	}
#ifdef USE_DST_BLACKLIST_STATS
	/* preinitializing before the number of processes is determined */
	if (init_dst_blacklist_stats(1)<0){
		LM_CRIT("could not initialize the dst blacklist measurement\n");
		goto error;
	}
#endif /* USE_DST_BLACKLIST_STATS */
#endif
	if (init_avps()<0) goto error;
	if (rpc_init_time() < 0) goto error;

#ifdef USE_TCP
	if (!tcp_disable){
		/*init tcp*/
		if (init_tcp()<0){
			LM_CRIT("could not initialize tcp, exiting...\n");
			goto error;
		}
	}
#endif /* USE_TCP */
#ifdef USE_SCTP
	if (!sctp_disable){
		if (sctp_core_init()<0){
			LM_CRIT("Could not initialize sctp, exiting...\n");
			goto error;
		}
	}
#endif /* USE_SCTP */
	/* init_daemon? */
	if( !dont_fork && daemonize((log_name==0)?argv[0]:log_name, 1) < 0)
		goto error;
	if (install_sigs() != 0){
		fprintf(stderr, "ERROR: could not install the signal handlers\n");
		goto error;
	}

	if (disable_core_dump) set_core_dump(0, 0);
	else set_core_dump(1, shm_mem_size+pkg_mem_size+4*1024*1024);
	if (open_files_limit>0){
		if(increase_open_fds(open_files_limit)<0){
			fprintf(stderr, "ERROR: error could not increase file limits\n");
			goto error;
		}
	}
	if (mlock_pages)
		mem_lock_pages();

	if (real_time&4)
			set_rt_prio(rt_prio, rt_policy);

#ifdef USE_TCP
#ifdef USE_TLS
	if (!tls_disable){
		if (!tls_loaded()){
			LM_WARN("tls support enabled, but no tls engine "
						" available (forgot to load the tls module?)\n");
			LM_WARN("disabling tls...\n");
			tls_disable=1;
		} else {
			if (pre_init_tls()<0){
				LM_CRIT("could not pre-initialize tls, exiting...\n");
				goto error;
			}
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */
	
	if (init_modules() != 0) {
		fprintf(stderr, "ERROR: error while initializing modules\n");
		goto error;
	}
	
	/* initialize process_table, add core process no. (calc_proc_no()) to the
	 * processes registered from the modules*/
	if (init_pt(calc_proc_no())==-1)
		goto error;
#ifdef USE_TCP
#ifdef USE_TLS
	if (!tls_disable){
		if (!tls_loaded()){
			LM_WARN("tls support enabled, but no tls engine "
						" available (forgot to load the tls module?)\n");
			LM_WARN("disabling tls...\n");
			tls_disable=1;
		}
		/* init tls*/
		if (init_tls()<0){
			LM_CRIT("could not initialize tls, exiting...\n");
			goto error;
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */

	/* The total number of processes is now known, note that no
	 * function being called before this point may rely on the
	 * number of processes !
	 */
	LM_DBG("Expect (at least) %d kamailio processes in your process list\n",
			get_max_procs());

#if defined USE_DNS_CACHE && defined USE_DNS_CACHE_STATS
	if (init_dns_cache_stats(get_max_procs())<0){
		LM_CRIT("could not initialize the dns cache measurement\n");
		goto error;
	}
#endif
#if defined USE_DST_BLACKLIST && defined USE_DST_BLACKLIST_STATS
	if (init_dst_blacklist_stats(get_max_procs())<0){
		LM_CRIT("could not initialize the dst blacklist measurement\n");
		goto error;
	}
#endif

	/* fix routing lists */
	if ( (r=fix_rls())!=0){
		fprintf(stderr, "error %d while trying to fix configuration\n", r);
		goto error;
	};
	fixup_complete=1;

#ifdef STATS
	if (init_stats(  dont_fork ? 1 : children_no  )==-1) goto error;
#endif

	ret=main_loop();
	if (ret < 0)
		goto error;
	/*kill everything*/
	if (is_main) shutdown_children(SIGTERM, 0);
	if (!dont_daemonize) {
		if (daemon_status_send(0) < 0)
			fprintf(stderr, "error sending exit status: %s [%d]\n",
					strerror(errno), errno);
	}
	/* else terminate process */
	return ret;

error:
	/*kill everything*/
	if (is_main) shutdown_children(SIGTERM, 0);
	if (!dont_daemonize) {
		if (daemon_status_send((char)-1) < 0)
			fprintf(stderr, "error sending exit status: %s [%d]\n",
					strerror(errno), errno);
	}
	return -1;
}
