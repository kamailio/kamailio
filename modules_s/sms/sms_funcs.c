#include "sms_funcs.h"




int sms_extract_body(struct sip_msg *msg, str *body )
{
	if ( parse_headers(msg,HDR_EOH)==-1 )
	{
		LOG(L_ERR,"ERROR: sms_extract_body:unable to parse all headers!\n");
		goto error;
	}

	/* get the lenght from COntent-Lenght header */


	if ( strncmp(CRLF,msg->unparsed,CRLF_LEN)!=0 )
	{
		LOG(L_ERR,"ERROR: sms_extract_body:unable to detect the beginning"
			" of message body!\n ");
		goto error;
	}

	body->s = msg->unparsed + CRLF_LEN;
	body->len = msg->buf + msg->len - body->s;
	DBG("------------- body =\n|%.*s|\n %d\n",body->len,body->s);

	return 1;
error:
	return -1;
}
