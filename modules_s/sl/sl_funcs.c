/*
 * $Id$
 */

#include <netinet/in.h>
#include <netdb.h>
#include "../../forward.h"
#include "../../dprint.h"
#include "../../md5utils.h"
#include "../../msg_translator.h"
#include "../../udp_server.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "sl_funcs.h"


char           sl_tag[32];
unsigned int  *sl_timeout;

int sl_startup()
{
	str  src[5];

	/*some fix string*/
	src[0].s="one two three four -> testing!" ;
	src[0].len=30;
	/*some fix string*/
	src[1].s="Sip Express router - sex" ;
	src[1].len=24;
	/*some fix string*/
	src[2].s="I love you men!!!! (Jiri's idea :))";
	src[2].len=35;
	/*proxy's IP*/
	src[3].s=(char*)addresses ;
	src[3].len=4;
	/*proxy's port*/
	src[4].s=port_no_str;
	src[4].len=port_no_str_len;
	MDStringArray ( sl_tag, src, 5 );

	/*timeout*/
	sl_timeout = (unsigned int*)shm_malloc(sizeof(unsigned int));
	if (!sl_timeout)
	{
		LOG(L_ERR,"ERROR:sl_startup: no more free memory!\n");
		return -1;
	}
	*(sl_timeout)=get_ticks();

	return 1;
}




int sl_send_reply(struct sip_msg *msg ,int code ,char *text )
{
	char               *buf;
	unsigned int       len;
	struct sockaddr_in to;

	if ( msg->first_line.u.request.method_value==METHOD_ACK)
	{
		DBG("DEBUG: sl_send_reply: I wan't send a reply for ACK!!\n");
		goto error;
	}
	to.sin_family = AF_INET;
	if (update_sock_struct_from_via(  &(to),  msg->via1 )==-1)
	{
		LOG(L_ERR, "ERROR: sl_send_reply: cannot lookup reply dst: %s\n",
			msg->via1->host.s );
		goto error;
	}
	/* To header is needed (tag param in particular)*/
	if (parse_headers(msg,HDR_TO)==-1 || msg->to==0)
	{
		LOG(L_ERR, "ERROR: sl_send_reply: cannot find/parse To\n");
		goto error;
	}
	/* to:tag is added only for INVITEs without To tag in order
	to be able to capture the ACK*/
	if ( msg->first_line.u.request.method_value==METHOD_INVITE
	&& (get_to(msg)->tag_value.s==0 || get_to(msg)->tag_value.len==0) )
		buf = build_res_buf_from_sip_req(code,text,sl_tag,32,msg ,&len);
	else
		buf = build_res_buf_from_sip_req(code,text,0,0,msg ,&len);
	if (!buf)
	{
		DBG("DEBUG: sl_send_reply: response building failed\n");
		goto error;
	}

	udp_send(buf,len,(struct sockaddr*)&(to),sizeof(struct sockaddr_in));
	pkg_free(buf);
	*(sl_timeout) = get_ticks() + SL_RPL_WAIT_TIME;

	return 1;

error:
	return -1;
}



/* Returns:
    0  : ACK to a local reply
    -1 : error
    1  : is not an ACK  or a non-local ACK
*/
int sl_filter_ACK(struct sip_msg *msg )
{
	str *tag_str;

	if (msg->first_line.u.request.method_value!=METHOD_ACK)
		goto pass_it;

	/*check the timeout value*/
	if ( *(sl_timeout)<= get_ticks() )
	{
		DBG("DEBUG : sl_filter_ACK: to late to be a local ACK!\n");
		goto pass_it;
	}

	/*force to parse to header -> we need it for tag param*/
	if (parse_headers( msg, HDR_TO )==-1)
	{
		LOG(L_ERR,"ERROR : SL_FILTER_ACK: unable to parse To header\n");
		return -1;
	}

	tag_str = &(get_to(msg)->tag_value);
	if ( tag_str->len==32 && !memcmp(tag_str->s,sl_tag,32) )
	{
		DBG("DEBUG: sl_filter_ACK : local ACK found -> dropping it! \n" );
		return 0;
	}

pass_it:
	return 1;
}







