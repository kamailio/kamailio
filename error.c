/*
 * $Id$
 *
 */

#include <stdio.h>
#include "error.h"

/* current function's error; */
int ser_error=-1;
/* previous error */
int prev_ser_error=-1;

int err2reason_phrase( 
	int ser_error,  /* current itnernal ser error */
	int *sip_error,  /* the sip error code to which ser 	
					    ser error will be turned */
	char *phrase,    /* resulting error text */
	int etl, 		/* error text buffer length */
	char *signature ) /* extra text to be appended */
{

	char *error_txt;

	switch( ser_error ) {
		case E_OUT_OF_MEM:
			error_txt="Excuse me I ran out of memory";
			*sip_error=500;
			break;
		case E_SEND:
			error_txt="Unfortunately error on sending to next hop occured";
			*sip_error=-ser_error;
			break;
		case E_BAD_ADDRESS:
			error_txt="Unresolveable destination";
			*sip_error=-ser_error;
			break;
		case E_BAD_REQ:
			error_txt="Bad Request";
			*sip_error=-ser_error;
			break;
		case E_BAD_URI:
			error_txt="Regretfuly, we were not able to process the URI";
			*sip_error=-ser_error;
			break;
		default:
			error_txt="I'm terribly sorry, server error occured";
			*sip_error=500;
			break;
	}
	return snprintf( phrase, etl, "%s (%d/%s)", error_txt, 
		-ser_error, signature );
}
