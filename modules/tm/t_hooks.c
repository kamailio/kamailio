/*
 * $Id$
 */

#include "stdlib.h"
#include "../../dprint.h"
#include "../../error.h"
#include "t_hooks.h"

static struct tm_callback_s* callback_array[ TMCB_END ] = { 0, 0 } ;
static int callback_id=0;

/* register a callback function 'f' of type 'cbt'; will be called
   back whenever the event 'cbt' occurs in transaction module
*/
int register_tmcb( tmcb_type cbt, transaction_cb f, void *param )
{
	struct tm_callback_s *cbs;

	if (cbt<0 || cbt>=TMCB_END ) {
		LOG(L_ERR, "ERROR: register_tmcb: invalid callback type: %d\n",
			cbt );
		return E_BUG;
	}

	if (!(cbs=malloc( sizeof( struct tm_callback_s)))) {
		LOG(L_ERR, "ERROR: register_tmcb: out of mem\n");
		return E_OUT_OF_MEM;
	}

	callback_id++;
	cbs->id=callback_id;
	cbs->callback=f;
	cbs->next=callback_array[ cbt ];
	cbs->param=param;
	callback_array[ cbt ]=cbs;

	return callback_id;
}

void callback_event( tmcb_type cbt , struct cell *trans,
	struct sip_msg *msg, int code )
{
	struct tm_callback_s *cbs;

	for (cbs=callback_array[ cbt ]; cbs; cbs=cbs->next)  {
		DBG("DBG: callback type %d, id %d entered\n", cbt, cbs->id );
		cbs->callback( trans, msg, code, cbs->param );
	}
}
