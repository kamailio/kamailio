/*
 * $Id$
 *
 * Script callbacks -- they add the ability to register callback
 * functions which are always called when script for request
 * processing is entered or left
 *
 */

#include <stdlib.h>
#include "script_cb.h"
#include "dprint.h"
#include "error.h"

static struct script_cb *pre_cb=0;
static struct script_cb *post_cb=0;
static unsigned int cb_id=0;

int register_script_cb( cb_function f, callback_t t, void *param )
{
	struct script_cb *new_cb;

	new_cb=malloc(sizeof(struct script_cb));
	if (new_cb==0) {
		LOG(L_ERR, "ERROR: register_script_cb: out of memory\n");
		return E_OUT_OF_MEM;
	}
	new_cb->cbf=f;
	new_cb->id=cb_id++;
	new_cb->param=param;
	/* insert into appropriate list */
	if (t==PRE_SCRIPT_CB) {
		new_cb->next=pre_cb;
		pre_cb=new_cb;
	} else if (t==POST_SCRIPT_CB) {
		new_cb->next=post_cb;
		post_cb=new_cb;
	} else {
		LOG(L_CRIT, "ERROR: register_script_cb: unknown CB type\n");
		return E_BUG;
	}
	/* ok, callback installed */
	return 1;
}

void exec_pre_cb( struct sip_msg *msg)
{
	struct script_cb *i;
	for (i=pre_cb; i; i=i->next) i->cbf(msg, i->param);
}

void exec_post_cb( struct sip_msg *msg)
{
	struct script_cb *i;
	for (i=post_cb; i; i=i->next) i->cbf(msg, i->param);
}

