/*
 * $Id$
 *
 */

#ifndef proxy_h
#define proxy_h

#include <netdb.h>

struct proxy_l{
	struct proxy_l* next;
	char* name; /* original name */
	struct hostent host; /* addresses */
	unsigned short port;
	unsigned short reserved; /*align*/
	
	/* socket ? */

	int addr_idx;	/* crt. addr. idx. */
	int ok; /* 0 on error */
	/*statisticis*/
	int tx;
	int tx_bytes;
	int errors;
};

extern struct proxy_l* proxies;

struct proxy_l* add_proxy(char* name, unsigned short port);
struct proxy_l* mk_proxy(char* name, unsigned short port);
struct proxy_l* mk_proxy_from_ip(unsigned int ip, unsigned short port);
void free_proxy(struct proxy_l* p);


#endif

