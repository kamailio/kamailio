/*
 * $Id$
 *
 * message printing
 */

#include "hash_func.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"



#define  append_mem_block(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);


/* Builds a CANCEL request based on an INVITE request. CANCEL is send
 * to same address as the INVITE */
int t_build_and_send_CANCEL(struct cell *Trans,unsigned int branch)
{
	struct sip_msg      *p_msg;
	struct hdr_field    *hdr;
	char                *cancel_buf, *p, *via;
	unsigned int         len, via_len;

	if ( !Trans->uac[branch].rpl_received )
	{
		DBG("DEBUG: t_build_and_send_CANCEL: no response ever received"
			" : dropping local cancel! \n");
		return 1;
	}

	if (Trans->uac[branch].request.cancel!=NO_CANCEL)
	{
		DBG("DEBUG: t_build_and_send_CANCEL: branch (%d)was already canceled"
			" : dropping local cancel! \n",branch);
		return 1;
	}

	cancel_buf = 0;
	via = 0;
	p_msg = Trans->uas.request;

	len = 0;
	/*first line's len - CANCEL and INVITE has the same lenght */
	len += ( REQ_LINE(p_msg).version.s+REQ_LINE(p_msg).version.len)-
		REQ_LINE(p_msg).method.s+CRLF_LEN;
	/*check if the REQ URI was override */
	if (Trans->uac[branch].uri.s)
		len += Trans->uac[branch].uri.len - REQ_LINE(p_msg).uri.len;
	/*via*/
	if ( add_branch_label(Trans,p_msg,branch)==-1 )
		goto error;
	via = via_builder(p_msg , &via_len, Trans->uac[branch].request.send_sock );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM || hdr->type==HDR_CALLID || 
			hdr->type==HDR_CSEQ || hdr->type==HDR_TO )
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
	/* User Agent header*/
	len += USER_AGENT_LEN + CRLF_LEN;
	/* Content Lenght heder*/
	len += CONTENT_LEN_LEN + CRLF_LEN;
	/* end of message */
	len += CRLF_LEN;

	cancel_buf=sh_malloc( len+1 );
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: cannot allocate memory\n");
		goto error;
	}
	p = cancel_buf;

	/* first line -> do we have a new URI? */
	if (Trans->uac[branch].uri.s)
	{
		append_mem_block(p,REQ_LINE(p_msg).method.s,
			REQ_LINE(p_msg).uri.s-REQ_LINE(p_msg).method.s);
		append_mem_block(p,Trans->uac[branch].uri.s,
			Trans->uac[branch].uri.len);
		append_mem_block(p,REQ_LINE(p_msg).uri.s+REQ_LINE(p_msg).uri.len,
			REQ_LINE(p_msg).version.s+REQ_LINE(p_msg).version.len-
			(REQ_LINE(p_msg).uri.s+REQ_LINE(p_msg).uri.len))
	}else{
		append_mem_block(p,REQ_LINE(p_msg).method.s,
			REQ_LINE(p_msg).version.s+REQ_LINE(p_msg).version.len-
			REQ_LINE(p_msg).method.s);
	}
	/* changhing method name*/
	memcpy(cancel_buf, CANCEL , CANCEL_LEN );
	append_mem_block(p,CRLF,CRLF_LEN);
	/* insert our via */
	append_mem_block(p,via,via_len);

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if(hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_TO)
		{
			append_mem_block(p,hdr->name.s,
				((hdr->body.s+hdr->body.len)-hdr->name.s) );
			append_mem_block(p, CRLF, CRLF_LEN );
		}else if ( hdr->type==HDR_CSEQ )
		{
			append_mem_block(p,hdr->name.s,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			append_mem_block(p, CANCEL CRLF, CANCEL_LEN +CRLF_LEN );
		}
}

	/* User Agent header */
	append_mem_block(p,USER_AGENT,USER_AGENT_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* Content Lenght header*/
	append_mem_block(p,CONTENT_LEN,CONTENT_LEN_LEN);
	append_mem_block(p,CRLF,CRLF_LEN);
	/* end of message */
	append_mem_block(p,CRLF,CRLF_LEN);
	*p=0;

	if (Trans->uac[branch].request.cancel) {
		shm_free( cancel_buf );
		LOG(L_WARN, "send_cancel: Warning: CANCEL already sent out\n");
		goto error;
	}

	Trans->uac[branch].request.activ_type = TYPE_LOCAL_CANCEL;
	Trans->uac[branch].request.cancel = cancel_buf;
	Trans->uac[branch].request.cancel_len = len;

	/*sets and starts the FINAL RESPONSE timer */
	set_timer(hash_table,&(Trans->uac[branch].request.fr_timer),FR_TIMER_LIST);
	/* sets and starts the RETRANS timer */
	Trans->uac[branch].request.retr_list = RT_T1_TO_1;
	set_timer(hash_table,&(Trans->uac[branch].request.retr_timer),RT_T1_TO_1);
	DBG("DEBUG: T_build_and_send_CANCEL : sending cancel...\n");
	SEND_CANCEL_BUFFER( &(Trans->uac[branch].request) );

	pkg_free(via);
	return 1;
error:
	if (via) pkg_free(via);
	return -1;
}


/* Builds an ACK request based on an INVITE request. ACK is send
 * to same address */
char *build_ack(struct sip_msg* rpl,struct cell *trans,int branch,int *ret_len)
{
	struct sip_msg      *p_msg , *r_msg;
	struct hdr_field    *hdr;
	char                *ack_buf, *p, *via;
	unsigned int         len, via_len;

	ack_buf = 0;
	via =0;
	p_msg = trans->uas.request;
	r_msg = rpl;

	if ( parse_headers(rpl,HDR_TO)==-1 || !rpl->to )
	{
		LOG(L_ERR, "ERROR: t_build_ACK: "
			"cannot generate a HBH ACK if key HFs in reply missing\n");
		goto error;
	}

	len = 0;
	/*first line's len */
	len += 4/*reply code and one space*/+
		p_msg->first_line.u.request.version.len+CRLF_LEN;
	/*uri's len*/
	if (trans->uac[branch].uri.s)
		len += trans->uac[branch].uri.len +1;
	else
		len += p_msg->first_line.u.request.uri.len +1;
	/*adding branch param*/
	if ( add_branch_label( trans , trans->uas.request , branch)==-1 )
		goto error;
	/*via*/
	via = via_builder(p_msg , &via_len, trans->uac[branch].request.send_sock );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_ACK: "
			"no via header got from builder\n");
		goto error;
	}
	len+= via_len;
	/*headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
		if (hdr->type==HDR_FROM||hdr->type==HDR_CALLID||hdr->type==HDR_CSEQ)
			len += ((hdr->body.s+hdr->body.len ) - hdr->name.s ) + CRLF_LEN ;
		else if ( hdr->type==HDR_TO )
			len += ((r_msg->to->body.s+r_msg->to->body.len ) -
				r_msg->to->name.s ) + CRLF_LEN ;
	/* CSEQ method : from INVITE-> ACK */
	len -= 3  ;
	/* end of message */
	len += CRLF_LEN; /*new line*/

	ack_buf = sh_malloc(len+1);
	if (!ack_buf)
	{
		LOG(L_ERR, "ERROR: t_build_and_ACK: cannot allocate memory\n");
		goto error1;
	}
	p = ack_buf;

	/* first line */
	memcpy( p , "ACK " , 4);
	p += 4;
	/* uri */
	if ( trans->uac[branch].uri.s )
	{
		memcpy(p,trans->uac[branch].uri.s,trans->uac[branch].uri.len);
		p +=trans->uac[branch].uri.len;
	}else{
		memcpy(p,p_msg->orig+(p_msg->first_line.u.request.uri.s-p_msg->buf),
			p_msg->first_line.u.request.uri.len );
		p += p_msg->first_line.u.request.uri.len;
	}
	/* SIP version */
	*(p++) = ' ';
	memcpy(p,p_msg->orig+(p_msg->first_line.u.request.version.s-p_msg->buf),
		p_msg->first_line.u.request.version.len );
	p += p_msg->first_line.u.request.version.len;
	memcpy( p, CRLF, CRLF_LEN );
	p+=CRLF_LEN;

	/* insert our via */
	memcpy( p , via , via_len );
	p += via_len;

	/*other headers*/
	for ( hdr=p_msg->headers ; hdr ; hdr=hdr->next )
	{
		if ( hdr->type==HDR_FROM || hdr->type==HDR_CALLID  )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((hdr->body.s+hdr->body.len ) - hdr->name.s ) );
			p += ((hdr->body.s+hdr->body.len ) - hdr->name.s );
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_TO )
		{
			memcpy( p , r_msg->orig+(r_msg->to->name.s-r_msg->buf) ,
				((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s));
			p+=((r_msg->to->body.s+r_msg->to->body.len)-r_msg->to->name.s);
			memcpy( p, CRLF, CRLF_LEN );
			p+=CRLF_LEN;
		}
		else if ( hdr->type==HDR_CSEQ )
		{
			memcpy( p , p_msg->orig+(hdr->name.s-p_msg->buf) ,
				((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s));
			p+=((((struct cseq_body*)hdr->parsed)->method.s)-hdr->name.s);
			memcpy( p , "ACK" CRLF, 3+CRLF_LEN );
			p += 3+CRLF_LEN;
		}
	}

	/* end of message */
	memcpy( p , CRLF , CRLF_LEN );
	p += CRLF_LEN;

	pkg_free( via );
	DBG("DEBUG: t_build_ACK: ACK generated\n");

	*(ret_len) = p-ack_buf;
	return ack_buf;

error1:
	pkg_free(via );
error:
	return 0;
}



