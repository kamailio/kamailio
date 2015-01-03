/* 
 * Copyright (C) 2009 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/** Kamailio core :: sip message shared memory cloner.
 * @file
 * @ingroup core
 * Module: @ref core
 */

#include "sip_msg_clone.h"


#include "dprint.h"
#include "mem/mem.h"
#include "data_lump.h"
#include "data_lump_rpl.h"
#include "ut.h"
#include "parser/digest/digest.h"
#include "parser/parse_to.h"
#include "atomic_ops.h"

/* rounds to the first 4 byte multiple on 32 bit archs
 * and to the first 8 byte multiple on 64 bit archs */
#define ROUND4(s) \
	(((s)+(sizeof(char*)-1))&(~(sizeof(char*)-1)))

#define lump_len( _lump) \
	(ROUND4(sizeof(struct lump)) +\
	ROUND4(((_lump)->op==LUMP_ADD)?(_lump)->len:0))
#define lump_clone( _new,_old,_ptr) \
	{\
		(_new) = (struct lump*)(_ptr);\
		memcpy( (_new), (_old), sizeof(struct lump) );\
		(_new)->flags|=LUMPFLAG_SHMEM; \
		(_ptr)+=ROUND4(sizeof(struct lump));\
		if ( (_old)->op==LUMP_ADD) {\
			(_new)->u.value = (char*)(_ptr);\
			memcpy( (_new)->u.value , (_old)->u.value , (_old)->len);\
			(_ptr)+=ROUND4((_old)->len);}\
	}

/* length of the data lump structures */
#define LUMP_LIST_LEN(len, list) \
do { \
        struct lump* tmp, *chain; \
	chain = (list); \
	while (chain) \
	{ \
		(len) += lump_len(chain); \
		tmp = chain->before; \
		while ( tmp ) \
		{ \
			(len) += lump_len( tmp ); \
			tmp = tmp->before; \
		} \
		tmp = chain->after; \
		while ( tmp ) \
		{ \
			(len) += lump_len( tmp ); \
			tmp = tmp->after; \
		} \
		chain = chain->next; \
	} \
} while(0);

/* length of the reply lump structure */
#define RPL_LUMP_LIST_LEN(len, list) \
do { \
	struct lump_rpl* rpl_lump; \
	for(rpl_lump=(list);rpl_lump;rpl_lump=rpl_lump->next) \
		(len)+=ROUND4(sizeof(struct lump_rpl))+ROUND4(rpl_lump->text.len); \
} while(0);

/* clones data lumps */
#define CLONE_LUMP_LIST(anchor, list, _ptr) \
do { \
	struct lump* lump_tmp, *l; \
	struct lump** lump_anchor2, **a; \
	a = (anchor); \
	l = (list); \
	while (l) \
	{ \
		lump_clone( (*a) , l , (_ptr) ); \
		/*before list*/ \
		lump_tmp = l->before; \
		lump_anchor2 = &((*a)->before); \
		while ( lump_tmp ) \
		{ \
			lump_clone( (*lump_anchor2) , lump_tmp , (_ptr) ); \
			lump_anchor2 = &((*lump_anchor2)->before); \
			lump_tmp = lump_tmp->before; \
		} \
		/*after list*/ \
		lump_tmp = l->after; \
		lump_anchor2 = &((*a)->after); \
		while ( lump_tmp ) \
		{ \
			lump_clone( (*lump_anchor2) , lump_tmp , (_ptr) ); \
			lump_anchor2 = &((*lump_anchor2)->after); \
			lump_tmp = lump_tmp->after; \
		} \
		a = &((*a)->next); \
		l = l->next; \
	} \
} while(0)

/* clones reply lumps */
#define CLONE_RPL_LUMP_LIST(anchor, list, _ptr) \
do { \
	struct lump_rpl* rpl_lump; \
	struct lump_rpl** rpl_lump_anchor; \
	rpl_lump_anchor = (anchor); \
	for(rpl_lump=(list);rpl_lump;rpl_lump=rpl_lump->next) \
	{ \
		*(rpl_lump_anchor)=(struct lump_rpl*)(_ptr); \
		(_ptr)+=ROUND4(sizeof( struct lump_rpl )); \
		(*rpl_lump_anchor)->flags = LUMP_RPL_SHMEM | \
			(rpl_lump->flags&(~(LUMP_RPL_NODUP|LUMP_RPL_NOFREE))); \
		(*rpl_lump_anchor)->text.len = rpl_lump->text.len; \
		(*rpl_lump_anchor)->text.s=(_ptr); \
		(_ptr)+=ROUND4(rpl_lump->text.len); \
		memcpy((*rpl_lump_anchor)->text.s,rpl_lump->text.s,rpl_lump->text.len); \
		(*rpl_lump_anchor)->next=0; \
		rpl_lump_anchor = &((*rpl_lump_anchor)->next); \
	} \
} while (0)



static inline struct via_body* via_body_cloner( char* new_buf,
					char *org_buf, struct via_body *param_org_via, char **p)
{
	struct via_body *new_via;
	struct via_body *first_via, *last_via;
	struct via_body *org_via;

	first_via = last_via = 0;
	org_via = param_org_via;

	do
	{
		/* clones the via_body structure */
		new_via = (struct via_body*)(*p);
		memcpy( new_via , org_via , sizeof( struct via_body) );
		(*p) += ROUND4(sizeof( struct via_body ));

		/* hdr (str type) */
		new_via->hdr.s=translate_pointer(new_buf,org_buf,org_via->hdr.s);
		/* name (str type) */
		new_via->name.s=translate_pointer(new_buf,org_buf,org_via->name.s);
		/* version (str type) */
		new_via->version.s=
			translate_pointer(new_buf,org_buf,org_via->version.s);
		/* transport (str type) */
		new_via->transport.s=
			translate_pointer(new_buf,org_buf,org_via->transport.s);
		/* host (str type) */
		new_via->host.s=translate_pointer(new_buf,org_buf,org_via->host.s);
		/* port_str (str type) */
		new_via->port_str.s=
			translate_pointer(new_buf,org_buf,org_via->port_str.s);
		/* params (str type) */
		new_via->params.s=translate_pointer(new_buf,org_buf,org_via->params.s);
		/* transaction id */
		new_via->tid.s=
			translate_pointer(new_buf, org_buf, org_via->tid.s);
		/* comment (str type) */
		new_via->comment.s=
			translate_pointer(new_buf,org_buf,org_via->comment.s);

		if ( org_via->param_lst )
		{
			struct via_param *vp, *new_vp, *last_new_vp;
			for( vp=org_via->param_lst, last_new_vp=0 ; vp ; vp=vp->next )
			{
				new_vp = (struct via_param*)(*p);
				memcpy( new_vp , vp , sizeof(struct via_param));
				(*p) += ROUND4(sizeof(struct via_param));
				new_vp->name.s=translate_pointer(new_buf,org_buf,vp->name.s);
				new_vp->value.s=translate_pointer(new_buf,org_buf,vp->value.s);
				new_vp->start=translate_pointer(new_buf,org_buf,vp->start);

				/* "translate" the shortcuts */
				switch(new_vp->type){
					case PARAM_BRANCH:
							new_via->branch = new_vp;
							break;
					case PARAM_RECEIVED:
							new_via->received = new_vp;
							break;
					case PARAM_RPORT:
							new_via->rport = new_vp;
							break;
					case PARAM_I:
							new_via->i = new_vp;
							break;
					case PARAM_ALIAS:
							new_via->alias = new_vp;
							break;

#ifdef USE_COMP
					case PARAM_COMP:
							new_via->comp = new_vp;
							break;
#endif
				}

				if (last_new_vp)
					last_new_vp->next = new_vp;
				else
					new_via->param_lst = new_vp;

				last_new_vp = new_vp;
				last_new_vp->next = NULL;
			}
			new_via->last_param = new_vp;
		}/*end if via has params */

		if (last_via)
			last_via->next = new_via;
		else
			first_via = new_via;
		last_via = new_via;
		org_via = org_via->next;
	}while(org_via);

	return first_via;
}


static void uri_trans(char *new_buf, char *org_buf, struct sip_uri *uri)
{
	uri->user.s=translate_pointer(new_buf,org_buf,uri->user.s);
	uri->passwd.s=translate_pointer(new_buf,org_buf,uri->passwd.s);
	uri->host.s=translate_pointer(new_buf,org_buf,uri->host.s);
	uri->port.s=translate_pointer(new_buf,org_buf,uri->port.s);
	uri->params.s=translate_pointer(new_buf,org_buf,uri->params.s);
	uri->headers.s=translate_pointer(new_buf,org_buf,uri->headers.s);
	/* parameters */
	uri->transport.s=translate_pointer(new_buf,org_buf,uri->transport.s);
	uri->ttl.s=translate_pointer(new_buf,org_buf,uri->ttl.s);
	uri->user_param.s=translate_pointer(new_buf,org_buf,uri->user_param.s);
	uri->maddr.s=translate_pointer(new_buf,org_buf,uri->maddr.s);
	uri->method.s=translate_pointer(new_buf,org_buf,uri->method.s);
	uri->lr.s=translate_pointer(new_buf,org_buf,uri->lr.s);
	uri->r2.s=translate_pointer(new_buf,org_buf,uri->r2.s);
	/* values */
	uri->transport_val.s
		=translate_pointer(new_buf,org_buf,uri->transport_val.s);
	uri->ttl_val.s=translate_pointer(new_buf,org_buf,uri->ttl_val.s);
	uri->user_param_val.s
		=translate_pointer(new_buf,org_buf,uri->user_param_val.s);
	uri->maddr_val.s=translate_pointer(new_buf,org_buf,uri->maddr_val.s);
	uri->method_val.s=translate_pointer(new_buf,org_buf,uri->method_val.s);
	uri->lr_val.s=translate_pointer(new_buf,org_buf,uri->lr_val.s);
	uri->r2_val.s=translate_pointer(new_buf,org_buf,uri->r2_val.s);
}


static inline struct auth_body* auth_body_cloner(char* new_buf, char *org_buf, struct auth_body *auth, char **p)
{
	struct auth_body* new_auth;

	new_auth = (struct auth_body*)(*p);
	memcpy(new_auth , auth , sizeof(struct auth_body));
	(*p) += ROUND4(sizeof(struct auth_body));

	/* authorized field must be cloned elsewhere */
	new_auth->digest.username.whole.s =
		translate_pointer(new_buf, org_buf, auth->digest.username.whole.s);
	new_auth->digest.username.user.s =
		translate_pointer(new_buf, org_buf, auth->digest.username.user.s);
	new_auth->digest.username.domain.s =
		translate_pointer(new_buf, org_buf, auth->digest.username.domain.s);
	new_auth->digest.realm.s =
		translate_pointer(new_buf, org_buf, auth->digest.realm.s);
	new_auth->digest.nonce.s =
		translate_pointer(new_buf, org_buf, auth->digest.nonce.s);
	new_auth->digest.uri.s =
		translate_pointer(new_buf, org_buf, auth->digest.uri.s);
	new_auth->digest.response.s =
		translate_pointer(new_buf, org_buf, auth->digest.response.s);
	new_auth->digest.alg.alg_str.s =
		translate_pointer(new_buf, org_buf, auth->digest.alg.alg_str.s);
	new_auth->digest.cnonce.s =
		translate_pointer(new_buf, org_buf, auth->digest.cnonce.s);
	new_auth->digest.opaque.s =
		translate_pointer(new_buf, org_buf, auth->digest.opaque.s);
	new_auth->digest.qop.qop_str.s =
		translate_pointer(new_buf, org_buf, auth->digest.qop.qop_str.s);
	new_auth->digest.nc.s =
		translate_pointer(new_buf, org_buf, auth->digest.nc.s);
	return new_auth;
}


static inline int clone_authorized_hooks(struct sip_msg* new,
					 struct sip_msg* old)
{
	struct hdr_field* ptr, *new_ptr, *hook1, *hook2;
	char stop = 0;

	get_authorized_cred(old->authorization, &hook1);
	if (!hook1) stop = 1;

	get_authorized_cred(old->proxy_auth, &hook2);
	if (!hook2) stop |= 2;

	ptr = old->headers;
	new_ptr = new->headers;

	while(ptr) {
		if (ptr == hook1) {
			if (!new->authorization || !new->authorization->parsed) {
				LM_CRIT("Error in message cloner (authorization)\n");
				return -1;
			}
			((struct auth_body*)new->authorization->parsed)->authorized =
				new_ptr;
			stop |= 1;
		}

		if (ptr == hook2) {
			if (!new->proxy_auth || !new->proxy_auth->parsed) {
				LM_CRIT("Error in message cloner (proxy_auth)\n");
				return -1;
			}
			((struct auth_body*)new->proxy_auth->parsed)->authorized =
				new_ptr;
			stop |= 2;
		}

		if (stop == 3) break;

		ptr = ptr->next;
		new_ptr = new_ptr->next;
	}
	return 0;
}


#define AUTH_BODY_SIZE sizeof(struct auth_body)

#define HOOK_SET(hook) (new_msg->hook != org_msg->hook)



/** Creates a shm clone for a sip_msg.
 * org_msg is cloned along with most of its headers and lumps into one
 * shm memory block (so that a shm_free() on the result will free everything)
 * @return shm malloced sip_msg on success, 0 on error
 * Warning: Cloner does not clone all hdr_field headers (From, To, etc.).
 */
struct sip_msg*  sip_msg_shm_clone( struct sip_msg *org_msg, int *sip_msg_len,
									int clone_lumps)
{
	unsigned int      len;
	struct hdr_field  *hdr,*new_hdr,*last_hdr;
	struct via_body   *via;
	struct via_param  *prm;
	struct to_param   *to_prm,*new_to_prm;
	struct sip_msg    *new_msg;
	char              *p;

	/*computing the length of entire sip_msg structure*/
	len = ROUND4(sizeof( struct sip_msg ));
	/*we will keep only the original msg +ZT */
	len += ROUND4(org_msg->len + 1);
	/*the new uri (if any)*/
	if (org_msg->new_uri.s && org_msg->new_uri.len)
		len+= ROUND4(org_msg->new_uri.len);
	/*the dst uri (if any)*/
	if (org_msg->dst_uri.s && org_msg->dst_uri.len)
		len+= ROUND4(org_msg->dst_uri.len);
	if (org_msg->path_vec.s && org_msg->path_vec.len)
			len+= ROUND4(org_msg->path_vec.len);
	/*all the headers*/
	for( hdr=org_msg->headers ; hdr ; hdr=hdr->next )
	{
		/*size of header struct*/
		len += ROUND4(sizeof( struct hdr_field));
		switch (hdr->type) {
			     /* Safely ignore auxiliary header types */
		case HDR_ERROR_T:
		case HDR_OTHER_T:
		case HDR_VIA2_T:
		case HDR_EOH_T:
			break;

		case HDR_VIA_T:
			for (via=(struct via_body*)hdr->parsed;via;via=via->next) {
				len+=ROUND4(sizeof(struct via_body));
				     /*via param*/
				for(prm=via->param_lst;prm;prm=prm->next)
					len+=ROUND4(sizeof(struct via_param ));
			}
			break;

		case HDR_TO_T:
		case HDR_FROM_T:
			     /* From header might be unparsed */
			if (hdr->parsed) {
				len+=ROUND4(sizeof(struct to_body));
				     /*to param*/
				to_prm = ((struct to_body*)(hdr->parsed))->param_lst;
				for(;to_prm;to_prm=to_prm->next)
					len+=ROUND4(sizeof(struct to_param ));
			}
			break;

		case HDR_CSEQ_T:
			len+=ROUND4(sizeof(struct cseq_body));
			break;


		case HDR_AUTHORIZATION_T:
		case HDR_PROXYAUTH_T:
			if (hdr->parsed) {
				len += ROUND4(AUTH_BODY_SIZE);
			}
			break;

		case HDR_CALLID_T:
		case HDR_CONTACT_T:
		case HDR_MAXFORWARDS_T:
		case HDR_ROUTE_T:
		case HDR_RECORDROUTE_T:
		case HDR_CONTENTTYPE_T:
		case HDR_CONTENTLENGTH_T:
		case HDR_RETRY_AFTER_T:
		case HDR_EXPIRES_T:
		case HDR_SUPPORTED_T:
		case HDR_REQUIRE_T:
		case HDR_PROXYREQUIRE_T:
		case HDR_UNSUPPORTED_T:
		case HDR_ALLOW_T:
		case HDR_EVENT_T:
		case HDR_ACCEPT_T:
		case HDR_ACCEPTLANGUAGE_T:
		case HDR_ORGANIZATION_T:
		case HDR_PRIORITY_T:
		case HDR_SUBJECT_T:
		case HDR_USERAGENT_T:
		case HDR_SERVER_T:
		case HDR_CONTENTDISPOSITION_T:
		case HDR_DIVERSION_T:
		case HDR_RPID_T:
		case HDR_REFER_TO_T:
		case HDR_SIPIFMATCH_T:
		case HDR_SESSIONEXPIRES_T:
		case HDR_MIN_SE_T:
		case HDR_SUBSCRIPTION_STATE_T:
		case HDR_ACCEPTCONTACT_T:
		case HDR_ALLOWEVENTS_T:
		case HDR_CONTENTENCODING_T:
		case HDR_REFERREDBY_T:
		case HDR_REJECTCONTACT_T:
		case HDR_REQUESTDISPOSITION_T:
		case HDR_WWW_AUTHENTICATE_T:
		case HDR_PROXY_AUTHENTICATE_T:
		case HDR_DATE_T:
		case HDR_IDENTITY_T:
		case HDR_IDENTITY_INFO_T:
		case HDR_PPI_T:
		case HDR_PAI_T:
		case HDR_PATH_T:
		case HDR_PRIVACY_T:
		case HDR_REASON_T:
			/* we ignore them for now even if they have something parsed*/
			break;
		}/*switch*/
	}/*for all headers*/
	
	if (clone_lumps) {
		/* calculate the length of the data and reply lump structures */
		LUMP_LIST_LEN(len, org_msg->add_rm);
		LUMP_LIST_LEN(len, org_msg->body_lumps);
		RPL_LUMP_LIST_LEN(len, org_msg->reply_lump);
	}
	
	p=(char *)shm_malloc(len);
	if (!p)
	{
		LM_ERR("cannot allocate memory\n" );
		return 0;
	}
	if (sip_msg_len)
		*sip_msg_len = len;

	/* filling up the new structure */
	new_msg = (struct sip_msg*)p;
	/* sip msg structure */
	memcpy( new_msg , org_msg , sizeof(struct sip_msg) );

	new_msg->msg_flags |= FL_SHM_CLONE;
	p += ROUND4(sizeof(struct sip_msg));
	new_msg->body = 0;
	new_msg->add_rm = 0;
	new_msg->body_lumps = 0;
	new_msg->reply_lump = 0;
	/* zero *uri.s, in case len is 0 but org_msg->*uris!=0 (just to be safe)*/
	new_msg->new_uri.s = 0;
	new_msg->dst_uri.s = 0;
	new_msg->path_vec.s = 0;
	/* new_uri */
	if (org_msg->new_uri.s && org_msg->new_uri.len)
	{
		new_msg->new_uri.s = p;
		memcpy( p , org_msg->new_uri.s , org_msg->new_uri.len);
		p += ROUND4(org_msg->new_uri.len);
	}
	/* dst_uri */
	if (org_msg->dst_uri.s && org_msg->dst_uri.len)
	{
		new_msg->dst_uri.s = p;
		memcpy( p , org_msg->dst_uri.s , org_msg->dst_uri.len);
		p += ROUND4(org_msg->dst_uri.len);
	}
	/* path vector */
	if (org_msg->path_vec.s && org_msg->path_vec.len) {
		new_msg->path_vec.s = p;
		memcpy(p, org_msg->path_vec.s, org_msg->path_vec.len);
		p += ROUND4(org_msg->path_vec.len);
	}

	/* instance is not cloned (it's reset instead) */
	new_msg->instance.s=0;
	new_msg->instance.len=0;
	/* ruid is not cloned (it's reset instead) */
	new_msg->ruid.s=0;
	new_msg->ruid.len=0;
	/* location ua is not cloned (it's reset instead) */
	new_msg->location_ua.s=0;
	new_msg->location_ua.len=0;
	/* reg_id is not cloned (it's reset instead) */
	new_msg->reg_id=0;
	/* local data struct is not cloned (it's reset instead) */
	memset(&new_msg->ldv, 0, sizeof(msg_ldata_t));
	/* message buffers(org and scratch pad) */
	memcpy( p , org_msg->buf, org_msg->len);
	/* ZT to be safer */
	*(p+org_msg->len)=0;
	new_msg->buf = p;
	p += ROUND4(new_msg->len+1);
	/* unparsed and eoh pointer */
	new_msg->unparsed = translate_pointer(new_msg->buf ,org_msg->buf,
		org_msg->unparsed );
	new_msg->eoh = translate_pointer(new_msg->buf,org_msg->buf,org_msg->eoh);
	/* first line, updating the pointers*/
	if ( org_msg->first_line.type==SIP_REQUEST )
	{
		new_msg->first_line.u.request.method.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.request.method.s );
		new_msg->first_line.u.request.uri.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.request.uri.s );
		new_msg->first_line.u.request.version.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.request.version.s );
		uri_trans(new_msg->buf, org_msg->buf, &new_msg->parsed_orig_ruri);
		if (org_msg->new_uri.s && org_msg->new_uri.len)
			uri_trans(new_msg->new_uri.s, org_msg->new_uri.s,
											&new_msg->parsed_uri);
		else
			uri_trans(new_msg->buf, org_msg->buf, &new_msg->parsed_uri);
	}
	else if ( org_msg->first_line.type==SIP_REPLY )
	{
		new_msg->first_line.u.reply.version.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.reply.version.s );
		new_msg->first_line.u.reply.status.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.reply.status.s );
		new_msg->first_line.u.reply.reason.s =
			translate_pointer( new_msg->buf , org_msg->buf ,
			org_msg->first_line.u.reply.reason.s );
	}

       /*headers list*/
       new_msg->via1=0;
       new_msg->via2=0;

	for( hdr=org_msg->headers,last_hdr=0 ; hdr ; hdr=hdr->next )
	{
		new_hdr = (struct hdr_field*)p;
		memcpy(new_hdr, hdr, sizeof(struct hdr_field) );
		p += ROUND4(sizeof( struct hdr_field));
		new_hdr->name.s = translate_pointer(new_msg->buf, org_msg->buf,
			hdr->name.s);
		new_hdr->body.s = translate_pointer(new_msg->buf, org_msg->buf,
			hdr->body.s);
		/* by default, we assume we don't understand this header in TM
		   and better set it to zero; if we do, we will set a specific
		   value in the following switch statement
		*/
		new_hdr->parsed=0;

		switch (hdr->type)
		{
			     /* Ignore auxiliary header types */
		case HDR_ERROR_T:
		case HDR_OTHER_T:
		case HDR_VIA2_T:
		case HDR_EOH_T:
		case HDR_ACCEPTCONTACT_T:
		case HDR_ALLOWEVENTS_T:
		case HDR_CONTENTENCODING_T:
		case HDR_REFERREDBY_T:
		case HDR_REJECTCONTACT_T:
		case HDR_REQUESTDISPOSITION_T:
		case HDR_WWW_AUTHENTICATE_T:
		case HDR_PROXY_AUTHENTICATE_T:
		case HDR_DATE_T:
		case HDR_IDENTITY_T:
		case HDR_IDENTITY_INFO_T:
		case HDR_RETRY_AFTER_T:
		case HDR_REASON_T:
			break;

		case HDR_VIA_T:
			if ( !new_msg->via1 ) {
				new_msg->h_via1 = new_hdr;
				new_msg->via1 = via_body_cloner(new_msg->buf,
								org_msg->buf, (struct via_body*)hdr->parsed, &p);
				new_hdr->parsed  = (void*)new_msg->via1;
				if ( new_msg->via1->next ) {
					new_msg->via2 = new_msg->via1->next;
				}
			} else if ( !new_msg->via2 && new_msg->via1 ) {
				new_msg->h_via2 = new_hdr;
				if ( new_msg->via1->next ) {
					new_hdr->parsed = (void*)new_msg->via1->next;
				} else {
					new_msg->via2 = via_body_cloner( new_msg->buf,
									 org_msg->buf, (struct via_body*)hdr->parsed, &p);
					new_hdr->parsed  = (void*)new_msg->via2;
				}
			} else if ( new_msg->via2 && new_msg->via1 ) {
				new_hdr->parsed = via_body_cloner( new_msg->buf , org_msg->buf ,
								   (struct via_body*)hdr->parsed , &p);
			}
			break;
		case HDR_CSEQ_T:
			new_hdr->parsed = p;
			p +=ROUND4(sizeof(struct cseq_body));
			memcpy(new_hdr->parsed, hdr->parsed, sizeof(struct cseq_body));
			((struct cseq_body*)new_hdr->parsed)->number.s =
				translate_pointer(new_msg->buf ,org_msg->buf,
						  ((struct cseq_body*)hdr->parsed)->number.s );
			((struct cseq_body*)new_hdr->parsed)->method.s =
				translate_pointer(new_msg->buf ,org_msg->buf,
						  ((struct cseq_body*)hdr->parsed)->method.s );
			if (!HOOK_SET(cseq)) new_msg->cseq = new_hdr;
			break;
		case HDR_TO_T:
		case HDR_FROM_T:
			if (hdr->type == HDR_TO_T) {
				if (!HOOK_SET(to)) new_msg->to = new_hdr;
			} else {
				if (!HOOK_SET(from)) new_msg->from = new_hdr;
			}
			     /* From header might be unparsed */
			if (!hdr->parsed) break;
			new_hdr->parsed = p;
			p +=ROUND4(sizeof(struct to_body));
			memcpy(new_hdr->parsed, hdr->parsed, sizeof(struct to_body));
			((struct to_body*)new_hdr->parsed)->body.s =
				translate_pointer( new_msg->buf , org_msg->buf ,
						   ((struct to_body*)hdr->parsed)->body.s );
			((struct to_body*)new_hdr->parsed)->display.s =
				translate_pointer( new_msg->buf, org_msg->buf,
						   ((struct to_body*)hdr->parsed)->display.s);
			((struct to_body*)new_hdr->parsed)->uri.s =
				translate_pointer( new_msg->buf , org_msg->buf ,
						   ((struct to_body*)hdr->parsed)->uri.s );
			if ( ((struct to_body*)hdr->parsed)->tag_value.s )
				((struct to_body*)new_hdr->parsed)->tag_value.s =
					translate_pointer( new_msg->buf , org_msg->buf ,
							   ((struct to_body*)hdr->parsed)->tag_value.s );
			if ( (((struct to_body*)new_hdr->parsed)->parsed_uri.user.s)
				|| (((struct to_body*)new_hdr->parsed)->parsed_uri.host.s) )
					uri_trans(new_msg->buf, org_msg->buf,
							&((struct to_body*)new_hdr->parsed)->parsed_uri);
			     /*to params*/
			to_prm = ((struct to_body*)(hdr->parsed))->param_lst;
			for(;to_prm;to_prm=to_prm->next) {
				     /*alloc*/
				new_to_prm = (struct to_param*)p;
				p +=ROUND4(sizeof(struct to_param ));
				     /*coping*/
				memcpy( new_to_prm, to_prm, sizeof(struct to_param ));
				((struct to_body*)new_hdr->parsed)->param_lst = 0;
				new_to_prm->name.s = translate_pointer( new_msg->buf,
									org_msg->buf , to_prm->name.s );
				new_to_prm->value.s = translate_pointer( new_msg->buf,
									 org_msg->buf , to_prm->value.s );
				     /*linking*/
				if ( !((struct to_body*)new_hdr->parsed)->param_lst )
					((struct to_body*)new_hdr->parsed)->param_lst
						= new_to_prm;
				else
					((struct to_body*)new_hdr->parsed)->last_param->next
						= new_to_prm;
				((struct to_body*)new_hdr->parsed)->last_param
					= new_to_prm;
			}
			break;
		case HDR_CALLID_T:
			if (!HOOK_SET(callid)) {
				new_msg->callid = new_hdr;
			}
			break;
		case HDR_CONTACT_T:
			if (!HOOK_SET(contact)) {
				new_msg->contact = new_hdr;
			}
			break;
		case HDR_MAXFORWARDS_T:
			if (!HOOK_SET(maxforwards)) {
				new_msg->maxforwards = new_hdr;
				new_msg->maxforwards->parsed = hdr->parsed;
			}
			break;
		case HDR_ROUTE_T:
			if (!HOOK_SET(route)) {
				new_msg->route = new_hdr;
			}
			break;
		case HDR_RECORDROUTE_T:
			if (!HOOK_SET(record_route)) {
				new_msg->record_route = new_hdr;
			}
			break;
		case HDR_CONTENTTYPE_T:
			if (!HOOK_SET(content_type)) {
				new_msg->content_type = new_hdr;
				new_msg->content_type->parsed = hdr->parsed;
			}
			break;
		case HDR_CONTENTLENGTH_T:
			if (!HOOK_SET(content_length)) {
				new_msg->content_length = new_hdr;
				new_msg->content_length->parsed = hdr->parsed;
			}
			break;
		case HDR_AUTHORIZATION_T:
			if (!HOOK_SET(authorization)) {
				new_msg->authorization = new_hdr;
			}
			if (hdr->parsed) {
				new_hdr->parsed = auth_body_cloner(new_msg->buf ,
								   org_msg->buf , (struct auth_body*)hdr->parsed , &p);
			}
			break;
		case HDR_EXPIRES_T:
			if (!HOOK_SET(expires)) {
				new_msg->expires = new_hdr;
			}
			break;
		case HDR_PROXYAUTH_T:
			if (!HOOK_SET(proxy_auth)) {
				new_msg->proxy_auth = new_hdr;
			}
			if (hdr->parsed) {
				new_hdr->parsed = auth_body_cloner(new_msg->buf ,
								   org_msg->buf , (struct auth_body*)hdr->parsed , &p);
			}
			break;
		case HDR_SUPPORTED_T:
			if (!HOOK_SET(supported)) {
				new_msg->supported = new_hdr;
			}
			break;
		case HDR_REQUIRE_T:
			if (!HOOK_SET(require)) {
				new_msg->require = new_hdr;
			}
			break;
		case HDR_PROXYREQUIRE_T:
			if (!HOOK_SET(proxy_require)) {
				new_msg->proxy_require = new_hdr;
			}
			break;
		case HDR_UNSUPPORTED_T:
			if (!HOOK_SET(unsupported)) {
				new_msg->unsupported = new_hdr;
			}
			break;
		case HDR_ALLOW_T:
			if (!HOOK_SET(allow)) {
				new_msg->allow = new_hdr;
			}
			break;
		case HDR_EVENT_T:
			if (!HOOK_SET(event)) {
				new_msg->event = new_hdr;
			}
			break;
		case HDR_ACCEPT_T:
			if (!HOOK_SET(accept)) {
				new_msg->accept = new_hdr;
			}
			break;
		case HDR_ACCEPTLANGUAGE_T:
			if (!HOOK_SET(accept_language)) {
				new_msg->accept_language = new_hdr;
			}
			break;
		case HDR_ORGANIZATION_T:
			if (!HOOK_SET(organization)) {
				new_msg->organization = new_hdr;
			}
			break;
		case HDR_PRIORITY_T:
			if (!HOOK_SET(priority)) {
				new_msg->priority = new_hdr;
			}
			break;
		case HDR_SUBJECT_T:
			if (!HOOK_SET(subject)) {
				new_msg->subject = new_hdr;
			}
			break;
		case HDR_USERAGENT_T:
			if (!HOOK_SET(user_agent)) {
				new_msg->user_agent = new_hdr;
			}
			break;
		case HDR_SERVER_T:
			if (!HOOK_SET(server)) {
				new_msg->server = new_hdr;
			}
			break;
		case HDR_CONTENTDISPOSITION_T:
			if (!HOOK_SET(content_disposition)) {
				new_msg->content_disposition = new_hdr;
			}
			break;
		case HDR_DIVERSION_T:
			if (!HOOK_SET(diversion)) {
				new_msg->diversion = new_hdr;
			}
			break;
		case HDR_RPID_T:
			if (!HOOK_SET(rpid)) {
				new_msg->rpid = new_hdr;
			}
			break;
		case HDR_REFER_TO_T:
			if (!HOOK_SET(refer_to)) {
				new_msg->refer_to = new_hdr;
			}
			break;
		case HDR_SESSIONEXPIRES_T:
			if (!HOOK_SET(session_expires)) {
				new_msg->session_expires = new_hdr;
			}
			break;
		case HDR_MIN_SE_T:
			if (!HOOK_SET(min_se)) {
				new_msg->min_se = new_hdr;
			}
			break;
		case HDR_SUBSCRIPTION_STATE_T:
			if (!HOOK_SET(subscription_state)) {
				new_msg->subscription_state = new_hdr;
			}
			break;
		case HDR_SIPIFMATCH_T:
			if (!HOOK_SET(sipifmatch)) {
				new_msg->sipifmatch = new_hdr;
			}
			break;
		case HDR_PPI_T:
			if (!HOOK_SET(ppi)) {
				new_msg->ppi = new_hdr;
			}
			break;
		case HDR_PAI_T:
			if (!HOOK_SET(pai)) {
				new_msg->pai = new_hdr;
			}
			break;
		case HDR_PATH_T:
			if (!HOOK_SET(path)) {
				new_msg->path = new_hdr;
			}
			break;
		case HDR_PRIVACY_T:
			if (!HOOK_SET(privacy)) {
				new_msg->privacy = new_hdr;
			}
			break;
		}/*switch*/

		if ( last_hdr )
		{
			last_hdr->next = new_hdr;
			last_hdr=last_hdr->next;
		}
		else
		{
			last_hdr=new_hdr;
			new_msg->headers =new_hdr;
		}
		last_hdr->next = 0;
		new_msg->last_header = last_hdr;
	}
	if (clone_lumps) {
		/*cloning data and reply lump structures*/
		CLONE_LUMP_LIST(&(new_msg->add_rm), org_msg->add_rm, p);
		CLONE_LUMP_LIST(&(new_msg->body_lumps), org_msg->body_lumps, p);
		CLONE_RPL_LUMP_LIST(&(new_msg->reply_lump), org_msg->reply_lump, p);
	}
	
	if (clone_authorized_hooks(new_msg, org_msg) < 0) {
		shm_free(new_msg);
		return 0;
	}

	return new_msg;
}



/** clones the data and reply lumps from pkg_msg to shm_msg.
 * A new memory block is allocated for the lumps (the lumps will point
 * into it).
 * Note: the new memory block is linked to add_rm if
 * at least one data lump is set, else it is linked to body_lumps
 * if at least one body lump is set, otherwise it is linked to
 * shm_msg->reply_lump.
 * @param pkg_msg - sip msg whoes lumps will be cloned
 * @param add_rm - result parameter, filled with the list of cloned
 *                 add_rm lumps (corresp. to msg->add_rm)
 * @param body_lumps - result parameter, filled with the list of cloned
 *                 body lumps (corresp. to msg->body_lumps)
 * @param reply_lump - result parameter, filled with the list of cloned
 *                 reply lumps (corresp. to msg->reply_lump)
 * @return 0 or 1 on success: 0 - lumps cloned), 1 - nothing to do and 
 *         -1 on error
 */
int msg_lump_cloner(struct sip_msg *pkg_msg,
					struct lump** add_rm,
					struct lump** body_lumps,
					struct lump_rpl** reply_lump)
{
	unsigned int	len;
	char		*p;

	*add_rm = *body_lumps = 0;
	*reply_lump = 0;

	/* calculate the length of the lumps */
	len = 0;
	LUMP_LIST_LEN(len, pkg_msg->add_rm);
	LUMP_LIST_LEN(len, pkg_msg->body_lumps);
	RPL_LUMP_LIST_LEN(len, pkg_msg->reply_lump);

	if (!len)
		return 1; /* nothing to do */

	p=(char *)shm_malloc(len);
	if (!p)
	{
		LM_ERR("cannot allocate memory\n" );
		return -1;
	}

	/* clone the lumps */
	CLONE_LUMP_LIST(add_rm, pkg_msg->add_rm, p);
	CLONE_LUMP_LIST(body_lumps, pkg_msg->body_lumps, p);
	CLONE_RPL_LUMP_LIST(reply_lump, pkg_msg->reply_lump, p);

	return 0;
}



/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
