#include <stdlib.h>
#include <string.h>

#include "mf_funcs.h"
#include "../../mem/mem.h"
#include "../../ut.h"


int  mf_global_id;
int  mf_hdr_value;


#define search_for_mf_hdr( _msg , _error ) \
	do{\
		if ( mf_global_id!=(_msg)->id ) {\
			mf_global_id  = (_msg)->id;\
			if ( !(_msg)->maxforwards ) {\
				DBG("DEBUG : search_for_mf_hdr : searching for max_forwards header\n");\
				if  ( parse_headers( _msg , HDR_MAXFORWARDS )==-1 ){\
					LOG( L_ERR , "ERROR: search_for_mf_header :"\
					  " parsing MAX_FORWARD header failed!\n");\
					goto _error;\
				}\
			}\
		}\
	}while(0);

#define get_number_from_str( _mf_str, _str,_x,_err) \
	do{\
		static char _c;\
		(_str)->len = (_mf_str).len;\
		(_str)->s    = (_mf_str).s;\
		while((_str)->len && ( (_c=(_str)->s[0])==0||_c==' '||_c=='\n'\
			||_c=='\r'||_c=='\t') ) {\
			(_str)->len--;\
			(_str)->s++;\
		}\
		while( (_str)->len && ( (_c=(_str)->s[(_str)->len-1])==0||_c==' '\
			||_c=='\n'||_c=='\r'||_c=='\t') ) \
			(_str)->len--;\
		*(_x) = str2s( (unsigned char*)(_str)->s,(_str)->len,(_err));\
	}while(0);


#define store_mf_hdr_value( _msg , _val ) \
	do{\
		mf_hdr_value = (_val);\
		mf_global_id   = (_msg)->id;\
	}while(0);

#define recall_mf_hdr_value( _msg , _val , _err) \
	do{\
		static str _foo;\
		if ( mf_global_id==(_msg)->id ){\
			*(_val) = mf_hdr_value;\
			*(_err) = 0;\
		}else{\
			get_number_from_str( (_msg)->maxforwards->body, &(_foo), _val, \
									(_err) );\
			mf_hdr_value = *(_val);\
			mf_global_id   = (_msg)->id;\
		}\
	}while(0);




int mf_startup()
{
	mf_global_id = 0;
	return 1;
}



int is_maxfwd_present( struct sip_msg* msg )
{
	search_for_mf_hdr( msg , error );
	return ( msg->maxforwards?1:-1);
error:
	return -1;
}



int decrement_maxfwd( struct sip_msg* msg )
{
	int          err, n;
	str          nr_s;
	unsigned int x;

	search_for_mf_hdr( msg , error );
	/*did we found the header after parsing?*/
	if ( !msg->maxforwards )
	{
		LOG( L_ERR , "ERROR: decrement_maxfwd :"
		  " MAX_FORWARDS header not found !\n");
		goto error;
	}

	/*extrancting the number from max-fwd header*/
	get_number_from_str( msg->maxforwards->body, &nr_s, &x, &err);
	if (err){
		LOG(L_ERR, "ERROR: decrement_maxfwd :"
		  " unable to parse the max forwards number !\n");
		goto error;
	}
	store_mf_hdr_value( msg , x );
	if (x==0){
		LOG(L_ERR, "ERROR: decrement_maxfwd :"
		  " unable to decrement max_fwd number -> already zero!\n");
		goto error;
	}

	/*rewritting the max-fwd value in the massage (buf and orig)*/
	n = btostr(nr_s.s,x-1);
	if ( n<nr_s.len )
		nr_s.s[n] = ' ';
	n = btostr(translate_pointer(msg->orig,msg->buf,nr_s.s),x-1);
	if ( n<nr_s.len )
		*(translate_pointer(msg->orig,msg->buf,nr_s.s+n)) = ' ';
	store_mf_hdr_value( msg , x-1 );
	return 1;

error:
	return -1;
}




int add_maxfwd_header( struct sip_msg* msg , unsigned int val )
{
	unsigned int  len;
	char          *buf;
	struct lump*  anchor;

	search_for_mf_hdr( msg , error );
	/*did we found the header after parsing?*/
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

	store_mf_hdr_value( msg , val );
	return 1;

error1:
	pkg_free( buf );
error:
	return -1;
}




int is_maxfwd_zero( struct sip_msg* msg )
{
	int err,x;

	search_for_mf_hdr( msg , error );
	/*did we found the header after parsing?*/
	if ( !msg->maxforwards )
	{
		LOG( L_ERR , "ERROR: is_maxfwd_zero :"
		  " MAX_FORWARDS header not found !\n");
		goto error;
	}

	recall_mf_hdr_value( msg , &x , &err);
	if (err){
		LOG(L_ERR, "ERROR: is_maxfwd_zero :"
		  " unable to parse the max forwards number !\n");
		goto error;
	}
	return (x==0)?1:-1;

error:
	return -1;

}



int reply_to_maxfwd_zero( struct sip_msg* msg )
{
	return 1;
}




