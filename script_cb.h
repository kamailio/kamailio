/*
 * $Id$
 */

#include "parser/msg_parser.h"

typedef int (cb_function)( struct sip_msg *msg, void *param );

typedef enum {
    PRE_SCRIPT_CB,
	POST_SCRIPT_CB
} callback_t;       /* Allowed types of callbacks */


struct script_cb{
	cb_function *cbf;
	struct script_cb *next;
	unsigned int id;
	void *param;
};

int register_script_cb( cb_function f, callback_t t, void *param );
void exec_pre_cb( struct sip_msg *msg);
void exec_post_cb( struct sip_msg *msg);


