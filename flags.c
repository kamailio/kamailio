/*
 * $Id$
 */

#include <limits.h>
#include "sr_module.h"
#include "dprint.h"
#include "parser/msg_parser.h"
#include "flags.h"
#include "error.h"
#include "stdlib.h"

int setflag( struct sip_msg* msg, flag_t flag ) {
	msg->flags |= 1 << flag;
	return 1;
}

int resetflag( struct sip_msg* msg, flag_t flag ) {
	msg->flags &= ~ (1 << flag);
	return 1;
}

int isflagset( struct sip_msg* msg, flag_t flag ) {
	return (msg->flags & (1<<flag)) ? 1 : -1;
}

int flag_in_range( flag_t flag ) {
	if (flag > MAX_FLAG ) {
		LOG(L_ERR, "ERROR: message flag %d too high; MAX=%d\n",
			flag, MAX_FLAG );
		return 0;
	}
	if (flag<=0) {
		LOG(L_ERR, "ERROR: message flag (%d) must be in range %d..%d\n",
			flag, 1, MAX_FLAG );
		return 0;
	}
	return 1;
}


#ifdef _GET_AWAY

/* wrapping functions for flag processing  */
static int fixup_t_flag(void** param, int param_no)
{
    unsigned int *code;
	char *c;
	int token;

	DBG("DEBUG: fixing flag: %s\n", (char *) (*param));

	if (param_no!=1) {
		LOG(L_ERR, "ERROR: TM module: only parameter #1 for flags can be fixed\n");
		return E_BUG;
	};

	if ( !(code = malloc( sizeof( unsigned int) )) ) return E_OUT_OF_MEM;

	*code = 0;
	c = *param;
	while ( *c && (*c==' ' || *c=='\t')) c++; /* intial whitespaces */

	token=1;
	if (strcasecmp(c, "white")==0) *code=FL_WHITE;
	else if (strcasecmp(c, "yellow")==0) *code=FL_YELLOW;
	else if (strcasecmp(c, "green")==0) *code=FL_GREEN;
	else if (strcasecmp(c, "red")==0) *code=FL_RED;
	else if (strcasecmp(c, "blue")==0) *code=FL_BLUE;
	else if (strcasecmp(c, "magenta")==0) *code=FL_MAGENTA;
	else if (strcasecmp(c, "brown")==0) *code=FL_BROWN;
	else if (strcasecmp(c, "black")==0) *code=FL_BLACK;
	else if (strcasecmp(c, "acc")==0) *code=FL_ACC;
	else {
		token=0;
		while ( *c && *c>='0' && *c<='9' ) {
			*code = *code*10+ *c-'0';
			if (*code > (sizeof( flag_t ) * CHAR_BIT - 1 )) {
				LOG(L_ERR, "ERROR: TM module: too big flag number: %s; MAX=%d\n",
					(char *) (*param), sizeof( flag_t ) * CHAR_BIT - 1 );
				goto error;
			}
			c++;
		}
	}
	while ( *c && (*c==' ' || *c=='\t')) c++; /* terminating whitespaces */

	if ( *code == 0 ) {
		LOG(L_ERR, "ERROR: TM module: bad flag number: %s\n", (char *) (*param));
		goto error;
	}

	if (*code < FL_MAX && token==0) {
		LOG(L_ERR, "ERROR: TM module: too high flag number: %s (%d)\n; lower number"
			" bellow %d reserved\n", (char *) (*param), *code, FL_MAX );
		goto error;
	}

	/* free string */
	free( *param );
	/* fix now */
	*param = code;
	
	return 0;

error:
	free( code );
	return E_CFG;
}


#endif
