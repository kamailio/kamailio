#include "mf_funcs.h"
#include "../../mem/mem.h"


int decrement_maxfed( struct sip_msg* msg )
{
   return 1;
}



int add_maxfwd_header( struct sip_msg* msg , unsigned int val )
{
	unsigned int len, val_len, foo;
	char               *buf, *bar, *bar1;
	struct lump* anchor;


	/* is there another MAX_FORWARD header? */
	if  ( parse_headers( msg , HDR_MAXFORWARDS )==-1 )
	{
		LOG( L_ERR , "ERROR: add_maxfwd_header :"
		  " parsing MAX_FORWARD header failed!\n");
		goto error;
	}

	/*did we found the header after parsing?*/
	if ( msg->maxforwards!=0 )
	{
		LOG( L_ERR , "ERROR: add_maxfwd_header :"
		  " MAX_FORWARDS header already exists !\n");
		goto error;
	}

	/* constructing the header */
	val_len = 1;
	len = 14 /*"MAX-FORWARDS: "*/+ CRLF_LEN + 1;
	for(foo=val;foo>=10; len++,foo=foo/10,val_len++);

	buf = (char*)pkg_malloc( len );
	if (!buf) {
		LOG(L_ERR, "ERROR : add_maxfwd_header : "
		  "No memory left\n");
		return -1;
	}
	bar = buf;
	memcpy( bar , "Max-Forwards: ", 14 );
	bar += 14 ;
	for(foo=val,bar1=bar+val_len-1;foo>0; foo=foo/10, bar1-- )
		*(bar1) = '0' + foo - 10*(foo/10);
	bar += val_len;
	memcpy( bar , CRLF , CRLF_LEN );

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



int is_maxfwd_zero( struct sip_msg* msg )
{
	return 1;
}



int reply_to_maxfwd_zero( struct sip_msg* msg )
{
	return 1;
}




