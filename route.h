/*
 * $Id$
 */

#ifndef route_h
#define route_h

#include <sys/types.h>
#include <regex.h>
#include <netdb.h>

#include "cfg_parser.h"

#define E_OUT_OF_MEM  -2
#define E_BAD_RE      -3
#define E_BAD_ADDRESS -4

struct route_elem{
	struct route_elem* next;
	regex_t method;
	regex_t uri;
	struct hostent host;
	int current_addr_idx;
	short int port;
	short int reserved; /* pad */
	int ok; /* set to 0 if an error was found sendig a pkt*/
	/*counters*/
	int errors;
	int tx;
	int tx_bytes;
};

/* main "routing table" */
extern struct route_elem* rlist;


void free_re(struct route_elem* re);
struct route_elem* init_re();
void push(struct route_elem* re, struct route_elem** head);
void clear_rlist(struct route_elem** rl);
int add_rule(struct cfg_line* cl, struct route_elem** head);
struct route_elem* route_match(char* method, char* uri, struct route_elem** rl);void print_rl();





#endif
