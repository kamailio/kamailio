/*
 * $Id$
 */

#ifndef _HOOKS_H
#define _HOOKS_H

#include "h_table.h"
#include "t_funcs.h"

typedef enum { TMCB_REPLY,  TMCB_E2EACK, TMCB_END } tmcb_type;

typedef void (transaction_cb) ( struct cell* t, struct sip_msg* msg );

struct tm_callback_s {
	int id;
	transaction_cb* callback;
	struct tm_callback_s* next;
};


extern struct tm_callback_s* callback_array[ TMCB_END ];

typedef int (*register_tmcb_f)(tmcb_type cbt, transaction_cb f);

int register_tmcb( tmcb_type cbt, transaction_cb f );
void callback_event( tmcb_type cbt, struct sip_msg *msg );

#endif
