#include "im_funcs.h"

#include "../../dprint.h"
#include "../../config.h"
#include "../../ut.h"



int inline im_get_body_len( struct sip_msg* msg)
{
	int x,err;
	str foo;

	if (!msg->content_length)
	{
		LOG(L_ERR,"ERROR: im_get_body_len: Content-Length header absent!\n");
		goto error;
	}
	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo.len , foo.s , msg->content_length->body );
	/* convert from string to number */
	x = str2s( (unsigned char*)foo.s,foo.len,&err);
	if (err){
		LOG(L_ERR, "ERROR: im_get_body_len:"
			" unable to parse the Content_Length number !\n");
		goto error;
	}
	return x;
error:
	return -1;
}




int im_check_content_type(struct sip_msg *msg)
{
	return 1;
}




int im_extract_body(struct sip_msg *msg, str *body )
{
	int len;

	if ( parse_headers(msg,HDR_EOH)==-1 )
	{
		LOG(L_ERR,"ERROR: im_extract_body:unable to parse all headers!\n");
		goto error;
	}

	/*is the content type corect?*/
	if (!im_check_content_type(msg))
	{
		LOG(L_ERR,"ERROR: im_extract_body: content type mismatching\n");
		goto error;
	}

	/* get the lenght from COntent-Lenght header */
	if ( (len = im_get_body_len(msg))<0 )
	{
		LOG(L_ERR,"ERROR: im_extract_body: cannot get body length\n");
		goto error;
	}

	if ( strncmp(CRLF,msg->unparsed,CRLF_LEN)!=0 )
	{
		LOG(L_ERR,"ERROR: im_extract_body:unable to detect the beginning"
			" of message body!\n ");
		goto error;
	}

	body->s = msg->unparsed + CRLF_LEN;
	body->len = len;
	DBG("im------------- body =\n|%.*s|\n",body->len,body->s);

	return 1;
error:
	return -1;
}




int im_get_user(struct sip_msg *msg, str *user, str *host)
{
	struct sip_uri uri;

	if (parse_uri(msg->first_line.u.request.uri.s,
		msg->first_line.u.request.uri.len, &uri) <0 )
	{
		LOG(L_ERR,"ERROR: im_get_user:unable to parse uri\n");
		return -1;
	}
	user = &uri.user;
	host = &uri.user;
	return -1;

}


