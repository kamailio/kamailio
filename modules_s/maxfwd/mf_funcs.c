#include "mf_funcs.h"
#include "../../mem/mem.h"
#include "../../ut.h"


#define MF_UNDEFINE  (struct hdr_field*)-1
#define MF_NULL           (struct hdr_field*)0



int                mf_global_id;
int                mf_hdr_value;



#define search_for_mf_hdr( _msg , _error ) \
	do{\
		if ( mf_global_id!=(_msg)->id ||\
		(_msg)->maxforwards==MF_UNDEFINE) {\
			mf_global_id = (_msg)->id;\
			(_msg)->maxforwards = MF_UNDEFINE;\
			if  ( parse_headers( _msg , HDR_MAXFORWARDS )==-1 ){\
				LOG( L_ERR , "ERROR: search_for_mf_header :"\
				  " parsing MAX_FORWARD header failed!\n");\
				(_msg)->maxforwards = MF_NULL;\
				goto _error;\
			}\
			if ( (_msg)->maxforwards==MF_UNDEFINE )\
				(_msg)->maxforwards=MF_NULL;\
		}\
	}while(0);




int mf_startup()
{
	mf_global_id = 0;
	return 1;
}







int decrement_maxfwd( struct sip_msg* msg )
{
	char              c;
	str                 mf_s;
	int                 err;
	unsigned int x;

	search_for_mf_hdr( msg , error );
	/*did we found the header after parsing?*/
	if ( msg->maxforwards==MF_NULL )
	{
		LOG( L_ERR , "ERROR: decrement_maxfwd :"
		  " MAX_FORWARDS header not found !\n");
		goto error;
	}

	mf_s.s    = msg->maxforwards->body.s;
	mf_s.len = msg->maxforwards->body.len;
	DBG("DEBUG: before DECREMENT ************************************\n");
	/*left trimming*/
	while( mf_s.len && ( (c=mf_s.s[0])==0||c==' '||c=='\n'||c=='\r'||c=='\t') )
	{
		mf_s.len--;
		mf_s.s++;
	}
	/*right trimming*/
	while( mf_s.len && ( (c=mf_s.s[mf_s.len-1])==0||c==' '||c=='\n'||c=='\r'||c=='\t') )
		mf_s.len--;
	x = str2s( mf_s.s,mf_s.len ,&err);
	DBG("DEBUG: DECREMENT : val = %d , err=%d\n",x,err);
	return 1;
error:
	return -1;
}



int add_maxfwd_header( struct sip_msg* msg , unsigned int val )
{
	unsigned int  len,a,b,c;
	char               *buf;
	struct lump* anchor;

	search_for_mf_hdr( msg , error );
	/*did we found the header after parsing?*/
	if ( msg->maxforwards!=MF_NULL )
	{
		LOG( L_ERR , "ERROR: add_maxfwd_header :"
		  " MAX_FORWARDS header already exists (%x) !\n",msg->maxforwards);
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

	mf_hdr_value = val;
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




