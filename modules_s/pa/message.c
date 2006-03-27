#include "pa_mod.h"
#include "message.h"
#include <cds/sstr.h>

int authorize_message(struct sip_msg* _m, char* _xcap_root, char*st)
{
	/* get and process XCAP authorization document */
	/* may modify the message or its body */

	int auth = 0;

	if (st) auth = atoi(st);
	if (auth) return 1;
	
	return -1;
}
