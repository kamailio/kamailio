#include "sms_funcs.h"




inline int sms_get_body_len( struct sip_msg* msg)
{
	int x,err;
	str foo;

	if (!msg->content_length)
	{
		LOG("ERROR: sms_get_body_len: Content-Length header absent!\n");
		goto error;
	}
	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo->len , foo->s , msg->content_length->body );
	/* convert from string to number */
	x = str2s( (unsigned char*)foo->s,foo->len,&err);
	if (err){
		LOG(L_ERR, "ERROR: sms_get_body_len:"
			" unable to parse the Content_Length number !\n");
		goto error;
	}
	return x;
error:
	return -1;
}




int sms_extract_body(struct sip_msg *msg, str *body )
{
	int len;

	if ( parse_headers(msg,HDR_EOH)==-1 )
	{
		LOG(L_ERR,"ERROR: sms_extract_body:unable to parse all headers!\n");
		goto error;
	}

	/* get the lenght from COntent-Lenght header */
	if ( (len = sms_get_body_len(msg))<0 )
	{
		LOG(L_ERR,"ERROR: sms_extract_body: cannot get body length\n");
		goto error:
	}

	if ( strncmp(CRLF,msg->unparsed,CRLF_LEN)!=0 )
	{
		LOG(L_ERR,"ERROR: sms_extract_body:unable to detect the beginning"
			" of message body!\n ");
		goto error;
	}

	body->s = msg->unparsed + CRLF_LEN;
	body->len = len;
	DBG("------------- body =\n|%.*s|\n",body->len,body->s);

	return 1;
error:
	return -1;
}


