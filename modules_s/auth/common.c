/*
 * $Id$
 *
 * Common functions needed by authorize
 * and challenge functions
 */

#include "common.h"
#include "../../data_lump_rpl.h"
#include "auth_mod.h"            /* sl_reply */


/*
 * Create a response with given code and reason phrase
 * Optionaly add new headers specified in _hdr
 */
int send_resp(struct sip_msg* _m, int _code, char* _reason, char* _hdr, int _hdr_len)
{
	struct lump_rpl* ptr;
	
	     /* Add new headers if there are any */
	if (_hdr) {
		ptr = build_lump_rpl(_hdr, _hdr_len);
		add_lump_rpl(_m, ptr);
	}
	
	sl_reply(_m, (char*)_code, _reason);
	return 0;
}
