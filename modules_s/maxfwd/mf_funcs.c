/*
 * $Id$
 */

#include <stdlib.h>
#include <string.h>

#include "mf_funcs.h"
#include "../../mem/mem.h"
#include "../../ut.h"



/* looks for the MAX FORWARDS header
   returns the its value, -1 if is not present or -2 for error */
int is_maxfwd_present( struct sip_msg* msg , str *foo)
{
	int x, err;
	char c;

	/* lookup into the message for MAX FORWARDS header*/
	if ( !msg->maxforwards ) {
		DBG("DEBUG : is_maxfwd_present: searching for max_forwards header\n");
		if  ( parse_headers( msg , HDR_MAXFORWARDS )==-1 ){
			LOG( L_ERR , "ERROR: is_maxfwd_present :"
				" parsing MAX_FORWARD header failed!\n");
			return -2;
		}
		if (!msg->maxforwards) {
			DBG("DEBUG: is_maxfwd_present: max_forwards header not found!\n");
			return -1;
		}
	}else{
		DBG("DEBUG : is_maxfwd_present: max_forward header already found!\n");
	}

	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo->len , foo->s , msg->maxforwards->body );
	/*foo->len = msg->maxforwards->body.len;
	foo->s   = msg->maxforwards->body.s;
	while (foo->len &&((c=foo->s[0])==0||c==' '||c=='\n'||c=='\r'||c=='\t'))
	{
		foo->len--;
		foo->s++;
	}
	while(foo->len && ((c=foo->s[foo->len-1])==0||c==' '||c=='\n'||c=='\r'
	||c=='\t') )
		foo->len--;*/

	/* convert from string to number */
	x = str2s( (unsigned char*)foo->s,foo->len,&err);
	if (err){
		LOG(L_ERR, "ERROR: is_maxfwd_zero :"
			" unable to parse the max forwards number !\n");
		return -2;
	}
	DBG("DEBUG: is_maxfwd_present: value = %d \n",x);
	return x;
}




int decrement_maxfwd( struct sip_msg* msg , int x, str *mf_val)
{
	int n;

	/* double check */
	if ( !msg->maxforwards )
	{
		LOG( L_ERR , "ERROR: decrement_maxfwd :"
		  " MAX_FORWARDS header not found !\n");
		goto error;
	}

	/*rewritting the max-fwd value in the message (buf and orig)*/
	n = btostr(mf_val->s,x-1);
	if ( n<mf_val->len )
		mf_val->s[n] = ' ';
	n = btostr(translate_pointer(msg->orig,msg->buf,mf_val->s),x-1);
	if ( n<mf_val->len )
		*(translate_pointer(msg->orig,msg->buf,mf_val->s+n)) = ' ';
	return 1;

error:
	return -1;
}




int add_maxfwd_header( struct sip_msg* msg , unsigned int val )
{
	unsigned int  len;
	char          *buf;
	struct lump*  anchor;

	/* double check just to be sure */
	if ( msg->maxforwards )
	{
		LOG( L_ERR , "ERROR: add_maxfwd_header :"
			" MAX_FORWARDS header already exists (%p) !\n",msg->maxforwards);
		goto error;
	}

	/* constructing the header */
	len = 14 /*"MAX-FORWARDS: "*/+ CRLF_LEN + 3/*val max on 3 digits*/;

	buf = (char*)pkg_malloc( len );
	if (!buf) {
		LOG(L_ERR, "ERROR : add_maxfwd_header : "
		  "No memory left\n");
		return -1;
	}
	memcpy( buf , "Max-Forwards: ", 14 );
	len = 14 ;
	len += btostr( buf+len , val );
	memcpy( buf+len , CRLF , CRLF_LEN );
	len +=CRLF_LEN;

	/*inserts the header at the begining of the message*/
	anchor = anchor_lump(&msg->add_rm, msg->headers->name.s - msg->buf, 0 , 0);
	if (anchor == 0) {
		LOG(L_ERR, "ERROR: add_maxfwd_header :"
		   " Error, can't get anchor\n");
		goto error1;
	}

	if (insert_new_lump_before(anchor, buf, len, 0) == 0) {
		LOG(L_ERR, "ERROR: add_maxfwd_header : "
		    "Error, can't insert MAX-FORWARDS\n");
		goto error1;
	}

	return 1;

error1:
	pkg_free( buf );
error:
	return -1;
}


