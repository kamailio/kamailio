/*
 * $Id$
 *
 */

#ifndef action_h
#define action_h

#include "msg_parser.h"
#include "route_struct.h"

int do_action(struct action* a, struct sip_msg* msg);
int run_actions(struct action* a, struct sip_msg* msg);





#endif
