/*
 * $Id$
 */

#ifndef route_h
#define route_h

#include <sys/types.h>
#include <regex.h>
#include <netdb.h>

#include "config.h"
#include "error.h"
#include "route_struct.h"
#include "msg_parser.h"

/*#include "cfg_parser.h" */



struct route_elem{
	struct route_elem* next;

	struct expr* condition;
	struct action* actions;

	int ok; /* set to 0 if an error was found sendig a pkt*/
	/*counters*/
	int errors;
	int tx;
	int tx_bytes;
};

/* main "routing table" */
extern struct route_elem* rlist[RT_NO];


void free_re(struct route_elem* re);
struct route_elem* init_re();
void push(struct route_elem* re, struct route_elem** head);
void clear_rlist(struct route_elem** rl);
int add_rule(struct expr* e, struct action* a, struct route_elem** head);
struct route_elem* route_match(struct sip_msg* msg,struct route_elem** rl);
void print_rl();





#endif
