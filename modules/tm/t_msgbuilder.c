/*
 * $Id$
 *
 * message printing
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * ----------
 * 2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-02-13  build_uac_request uses proto (andrei)
 * 2003-02-28  scratchpad compatibility abandoned (jiri)
 * 2003-04-14  build_local no longer checks reply status as it
 *             is now called before reply status is updated to
 *             avoid late ACK sending (jiri)
 * 2003-10-02  added via_builder set host/port support (andrei)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 */

#include "defs.h"


#include "../../comp_defs.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "t_msgbuilder.h"
#include "uac.h"



#define  append_mem_block(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);

#define append_str(_p,_str) \
	do{  \
		memcpy((_p), (_str).s, (_str).len); \
		(_p)+=(_str).len;  \
 	} while(0);

/* Build a local request based on a previous request; main
   customers of this function are local ACK and local CANCEL
 */
char *build_local(struct cell *Trans,unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to)
{
	char                *cancel_buf, *p, *via;
	unsigned int         via_len;
	struct hdr_field    *hdr;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	int branch_len;
	str branch_str;
	struct hostport hp;

#ifdef _OBSO
	if ( Trans->uac[branch].last_received<100)
	{
		DBG("DEBUG: build_local: no response ever received"
			" : dropping local request! \n");
		goto error;
	}
#endif

	/* method, separators, version: "CANCEL sip:p2@iptel.org SIP/2.0" */
	*len=SIP_VERSION_LEN + method_len + 2 /* spaces */ + CRLF_LEN;
	*len+=Trans->uac[branch].uri.len;

	/*via*/
	if (!t_calc_branch(Trans,  branch, 
		branch_buf, &branch_len ))
		goto error;
	branch_str.s=branch_buf;
	branch_str.len=branch_len;
	set_hostport(&hp, (Trans->local)?0:(Trans->uas.request));
	via=via_builder(&via_len, Trans->uac[branch].request.dst.send_sock,
		&branch_str, 0, Trans->uac[branch].request.dst.proto, &hp );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: "
			"no via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	/*headers*/
	*len+=Trans->from.len+Trans->callid.len+to->len+
		+Trans->cseq_n.len+1+method_len+CRLF_LEN; 


	/* copy'n'paste Route headers */
	if (!Trans->local) {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			 if (hdr->type==HDR_ROUTE)
				*len+=hdr->len;
	}

	/* User Agent */
	if (server_signature) {
		*len += USER_AGENT_LEN + CRLF_LEN;
	}
	/* Content Length, EoM */
	*len+=CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN;

	cancel_buf=shm_malloc( *len+1 );
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: cannot allocate memory\n");
		goto error01;
	}
	p = cancel_buf;

	append_mem_block( p, method, method_len );
	append_mem_block( p, " ", 1 );
	append_str( p, Trans->uac[branch].uri );
	append_mem_block( p, " " SIP_VERSION CRLF, 1+SIP_VERSION_LEN+CRLF_LEN );

	/* insert our via */
	append_mem_block(p,via,via_len);

	/*other headers*/
	append_str( p, Trans->from );
	append_str( p, Trans->callid );
	append_str( p, *to );

	append_str( p, Trans->cseq_n );
	append_mem_block( p, " ", 1 );
	append_mem_block( p, method, method_len );
	append_mem_block( p, CRLF, CRLF_LEN );

	if (!Trans->local)  {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			if(hdr->type==HDR_ROUTE) {
				append_mem_block(p, hdr->name.s, hdr->len );
			}
	}

	/* User Agent header */
	if (server_signature) {
		append_mem_block(p,USER_AGENT CRLF, USER_AGENT_LEN+CRLF_LEN );
	}
	/* Content Length, EoM */
	append_mem_block(p, CONTENT_LENGTH "0" CRLF CRLF ,
		CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN);
	*p=0;

	pkg_free(via);
	return cancel_buf;
error01:
	pkg_free(via);
error:
	return NULL;
}


/*
 * Convert lenght of body into asciiz
 */
static inline int print_content_length(str* dest, str* body)
{
	static char content_length[10];
	int len;
	char* tmp;

	     /* Print Content-Length */
	if (body) {
		tmp = int2str(body->len, &len);
		if (len >= sizeof(content_length)) {
			LOG(L_ERR, "ERROR: print_content_length: content_len too big\n");
			return -1;
		}
		memcpy(content_length, tmp, len); 
		dest->s = content_length;
		dest->len = len;
	} else {
		dest->s = 0;
		dest->len = 0;
	}
	return 0;
}


/*
 * Convert CSeq number into asciiz
 */
static inline int print_cseq_num(str* _s, dlg_t* _d)
{
	static char cseq[INT2STR_MAX_LEN];
	char* tmp;
	int len;

	tmp = int2str(_d->loc_seq.value, &len);
	if (len > sizeof(cseq)) {
		LOG(L_ERR, "print_cseq_num: cseq too big\n");
		return -1;
	}
	
	memcpy(cseq, tmp, len);
	_s->s = cseq;
	_s->len = len;
	return 0;
}


/*
 * Create Via header
 */
static inline int assemble_via(str* dest, struct cell* t, struct socket_info* sock, int branch)
{
	static char branch_buf[MAX_BRANCH_PARAM_LEN];
	char* via;
	int len;
	unsigned int via_len;
	str branch_str;
	struct hostport hp;

	if (!t_calc_branch(t, branch, branch_buf, &len)) {
		LOG(L_ERR, "ERROR: build_via: branch calculation failed\n");
		return -1;
	}
	
	branch_str.s = branch_buf;
	branch_str.len = len;

#ifdef XL_DEBUG
	printf("!!!proto: %d\n", sock->proto);
#endif

	set_hostport(&hp, 0);
	via = via_builder(&via_len, sock, &branch_str, 0, sock->proto, &hp);
	if (!via) {
		LOG(L_ERR, "build_via: via building failed\n");
		return -2;
	}
	
	dest->s = via;
	dest->len = via_len;
	return 0;
}


/*
 * Print Request-URI
 */
static inline char* print_request_uri(char* w, str* method, dlg_t* dialog, struct cell* t, int branch)
{
	memapp(w, method->s, method->len); 
	memapp(w, " ", 1); 

	t->uac[branch].uri.s = w; 
	t->uac[branch].uri.len = dialog->hooks.request_uri->len;

	memapp(w, dialog->hooks.request_uri->s, dialog->hooks.request_uri->len); 
	memapp(w, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);

	return w;
}


/*
 * Print To header field
 */
static inline char* print_to(char* w, dlg_t* dialog, struct cell* t)
{
	t->to.s = w;
	t->to.len = TO_LEN + dialog->rem_uri.len + CRLF_LEN;

	memapp(w, TO, TO_LEN);
	memapp(w, dialog->rem_uri.s, dialog->rem_uri.len);

	if (dialog->id.rem_tag.len) {
		t->to.len += TOTAG_LEN + dialog->id.rem_tag.len ;
		memapp(w, TOTAG, TOTAG_LEN);
		memapp(w, dialog->id.rem_tag.s, dialog->id.rem_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print From header field
 */
static inline char* print_from(char* w, dlg_t* dialog, struct cell* t)
{
	t->from.s = w;
	t->from.len = FROM_LEN + dialog->loc_uri.len + CRLF_LEN;

	memapp(w, FROM, FROM_LEN);
	memapp(w, dialog->loc_uri.s, dialog->loc_uri.len);

	if (dialog->id.loc_tag.len) {
		t->from.len += FROMTAG_LEN + dialog->id.loc_tag.len;
		memapp(w, FROMTAG, FROMTAG_LEN);
		memapp(w, dialog->id.loc_tag.s, dialog->id.loc_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print CSeq header field
 */
char* print_cseq_mini(char* target, str* cseq, str* method) {
	memapp(target, CSEQ, CSEQ_LEN);
	memapp(target, cseq->s, cseq->len);
	memapp(target, " ", 1);
	memapp(target, method->s, method->len);
	return target;
}

static inline char* print_cseq(char* w, str* cseq, str* method, struct cell* t)
{
	t->cseq_n.s = w; 
	/* don't include method name and CRLF -- subsequent
	 * local reuqests ACK/CANCEl will add their own */
	t->cseq_n.len = CSEQ_LEN + cseq->len; 
	w = print_cseq_mini(w, cseq, method);
	return w;
}

/*
 * Print Call-ID header field
 * created an extra function for pure header field creation, that is used by t_cancel for 
 * t_uac_cancel FIFO function.
 */
char* print_callid_mini(char* target, str callid) {
	memapp(target, CALLID, CALLID_LEN);
	memapp(target, callid.s, callid.len);
	memapp(target, CRLF, CRLF_LEN);
	return target;
}

static inline char* print_callid(char* w, dlg_t* dialog, struct cell* t)
{
	/* begins with CRLF, not included in t->callid, don`t know why...?!? */
	memapp(w, CRLF, CRLF_LEN);
	t->callid.s = w;
	t->callid.len = CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;

	w = print_callid_mini(w, dialog->id.call_id);
	return w;
}


/*
 * Create a request
 */
char* build_uac_req(str* method, str* headers, str* body, dlg_t* dialog, int branch, 
			struct cell *t, int* len, struct socket_info* send_sock)
{
	char* buf, *w;
	str content_length, cseq, via;

	if (!method || !dialog) {
		LOG(L_ERR, "build_uac_req(): Invalid parameter value\n");
		return 0;
	}
	if (print_content_length(&content_length, body) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing content-length\n");
		return 0;
	}
	if (print_cseq_num(&cseq, dialog) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing CSeq number\n");
		return 0;
	}
	*len = method->len + 1 + dialog->hooks.request_uri->len + 1 + SIP_VERSION_LEN + CRLF_LEN;

	if (assemble_via(&via, t, send_sock, branch) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while assembling Via\n");
		return 0;
	}
	*len += via.len;

	*len += TO_LEN + dialog->rem_uri.len
		+ (dialog->id.rem_tag.len ? (TOTAG_LEN + dialog->id.rem_tag.len) : 0) + CRLF_LEN;    /* To */
	*len += FROM_LEN + dialog->loc_uri.len
		+ (dialog->id.loc_tag.len ? (FROMTAG_LEN + dialog->id.loc_tag.len) : 0) + CRLF_LEN;  /* From */
	*len += CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;                                      /* Call-ID */
	*len += CSEQ_LEN + cseq.len + 1 + method->len + CRLF_LEN;                                    /* CSeq */
	*len += calculate_routeset_length(dialog);                                                   /* Route set */
	*len += (body ? (CONTENT_LENGTH_LEN + content_length.len + CRLF_LEN) : 0);                   /* Content-Length */
	*len += (server_signature ? (USER_AGENT_LEN + CRLF_LEN) : 0);                                /* Signature */
	*len += (headers ? headers->len : 0);                                                        /* Additional headers */
	*len += (body ? body->len : 0);                                                              /* Message body */
	*len += CRLF_LEN;                                                                            /* End of Header */

	buf = shm_malloc(*len + 1);
	if (!buf) {
		LOG(L_ERR, "build_uac_req(): no shmem\n");
		goto error;
	}
	
	w = buf;

	w = print_request_uri(w, method, dialog, t, branch);  /* Request-URI */
	memapp(w, via.s, via.len);                            /* Top-most Via */
	w = print_to(w, dialog, t);                           /* To */
	w = print_from(w, dialog, t);                         /* From */
	w = print_cseq(w, &cseq, method, t);                  /* CSeq */
	w = print_callid(w, dialog, t);                       /* Call-ID */
	w = print_routeset(w, dialog);                        /* Route set */

	     /* Content-Length */
	if (body) {
		memapp(w, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memapp(w, content_length.s, content_length.len);
		memapp(w, CRLF, CRLF_LEN);
	}
	
	     /* Server signature */
	if (server_signature) memapp(w, USER_AGENT CRLF, USER_AGENT_LEN + CRLF_LEN);
	if (headers) memapp(w, headers->s, headers->len);
	memapp(w, CRLF, CRLF_LEN);
     	if (body) memapp(w, body->s, body->len);

#ifdef EXTRA_DEBUG
	if (w-buf != *len ) abort();
#endif

	pkg_free(via.s);
	return buf;

 error:
	pkg_free(via.s);
	return 0;
}


int t_calc_branch(struct cell *t, 
	int b, char *branch, int *branch_len)
{
	return syn_branch ?
		branch_builder( t->hash_index,
			t->label, 0,
			b, branch, branch_len )
		: branch_builder( t->hash_index,
			0, t->md5,
			b, branch, branch_len );
}

