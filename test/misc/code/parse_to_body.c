/** to/from body parser test program. */
/* compile with:
    gcc -Wall parse_to_body.c -o parse_to_body -DFAST_LOCK -D__CPU_i386 */
#include <stdio.h>
#include <stdlib.h> /* exit() */
#include <string.h>
#include <stdarg.h>
#include "../str.h"

/* ser compat defs */
#define EXTRA_DEBUG
#include "../parser/parse_to.c"
#include "../dprint.c"


int ser_error=0;
int log_stderr=1;
int process_no=0;
struct process_table* pt=0;
int phone2tel=1;
/*volatile int dprint_crit=0; */
int my_pid() {return 0; };

struct cfg_group_core default_core_cfg = {
	L_DBG, /*  print only msg. < L_WARN */
	LOG_DAEMON,	/* log_facility -- see syslog(3) */
#ifdef USE_DST_BLACKLIST
	/* blacklist */
	0, /* dst blacklist is disabled by default */
	DEFAULT_BLST_TIMEOUT,
	DEFAULT_BLST_MAX_MEM,
#endif
	/* resolver */
	1,  /* dns_try_ipv6 -- on by default */
	0,  /* dns_try_naptr -- off by default */
	30,  /* udp transport preference (for naptr) */
	20,  /* tcp transport preference (for naptr) */
	10,  /* tls transport preference (for naptr) */
	20,  /* sctp transport preference (for naptr) */
	-1, /* dns_retr_time */
	-1, /* dns_retr_no */
	-1, /* dns_servers_no */
	1,  /* dns_search_list */
	1,  /* dns_search_fmatch */
	0,  /* dns_reinit */
	/* DNS cache */
#ifdef USE_DNS_CACHE
	1,  /* use_dns_cache -- on by default */
	0,  /* dns_cache_flags */
	0,  /* use_dns_failover -- off by default */
	0,  /* dns_srv_lb -- off by default */
	DEFAULT_DNS_NEG_CACHE_TTL, /* neg. cache ttl */
	DEFAULT_DNS_CACHE_MIN_TTL, /* minimum ttl */
	DEFAULT_DNS_CACHE_MAX_TTL, /* maximum ttl */
	DEFAULT_DNS_MAX_MEM, /* dns_cache_max_mem */
	0, /* dns_cache_del_nonexp -- delete only expired entries by default */
#endif
#ifdef PKG_MALLOC
	0, /* mem_dump_pkg */
#endif
#ifdef SHM_MEM
	0, /* mem_dump_shm */
#endif
};

void	*core_cfg = &default_core_cfg;

void dprint(char * format, ...)
{
	va_list ap;

	fprintf(stderr, "%2d(%d) ", process_no, my_pid());
	va_start(ap, format);
	vfprintf(stderr,format,ap);
	fflush(stderr);
	va_end(ap);
}



int main (int argc, char** argv)
{

	int r;
	struct to_body to_b;
	struct to_param *p;

	if (argc<2){
		printf("usage:    %s  to_body [, to_body...]\n", argv[0]);
		exit(1);
	}
	
	for (r=1; r<argc; r++){
		/*memset(&to_b, 0, sizeof(to_b));*/
		if (parse_to(argv[r], argv[r]+strlen(argv[r]), &to_b)==0 ||
				to_b.error!=1){
			printf("error: parsing %s\n", argv[r]);
			continue;
		}
		printf("body:    [%.*s]\n", to_b.body.len, to_b.body.s);
		printf("uri:     [%.*s]\n", to_b.uri.len, to_b.uri.s);
		printf("display: [%.*s]\n", to_b.display.len, to_b.display.s);
		printf("tag:     [%.*s]\n\n", to_b.tag_value.len, to_b.tag_value.s);
		for (p=to_b.param_lst; p; p=p->next){
			printf("	param type: %d\n", p->type);
			printf("	param name: [%.*s]\n", p->name.len, p->name.s);
			printf("	param value: [%.*s]\n\n", p->value.len, p->value.s);
		}
		printf("\n");
	}
	return 0;
}
