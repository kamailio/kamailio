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
#include "parser/msg_parser.h"

/*#include "cfg_parser.h" */


/* main "script table" */
extern struct action* rlist[RT_NO];
/* main reply route table */
extern struct action* reply_rlist[RT_NO];


void push(struct action* a, struct action** head);
int add_actions(struct action* a, struct action** head);
void print_rl();
int fix_rls();

int eval_expr(struct expr* e, struct sip_msg* msg);






#endif
