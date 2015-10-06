/*
 * Process REGISTER request and send reply
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/*!
 * \file
 * \brief SIP registrar module - Process REGISTER request and send reply
 * \ingroup registrar   
 */  


#include "../../str.h"
#include "../../socket_info.h"
#include "../../parser/parse_allow.h"
#include "../../parser/parse_methods.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../trim.h"
#include "../../ut.h"
#include "../../qvalue.h"
#include "../../dset.h"
#include "../../xavp.h"
#include "../../mod_fix.h"
#include "../../lib/srutils/sruid.h"
#include "../../lib/kcore/cmpapi.h"
#include "../../parser/parse_require.h"
#include "../../parser/parse_supported.h"
#include "../../lib/kcore/statistics.h"
#ifdef USE_TCP
#include "../../tcp_server.h"
#endif
#include "../usrloc/usrloc.h"
#include "common.h"
#include "sip_msg.h"
#include "rerrno.h"
#include "reply.h"
#include "reg_mod.h"
#include "regtime.h"
#include "path.h"
#include "save.h"
#include "config.h"

static int mem_only = 0;

extern sruid_t _reg_sruid;

static int q_override_msg_id;
static qvalue_t q_override_value;

/*! \brief
 * Process request that contained a star, in that case, 
 * we will remove all bindings with the given username 
 * from the usrloc and return 200 OK response
 */
static inline int star(sip_msg_t *_m, udomain_t* _d, str* _a, str *_h)
{
	urecord_t* r;
	ucontact_t* c;

	ul.lock_udomain(_d, _a);

	if (!ul.get_urecord(_d, _a, &r)) {
		c = r->contacts;
		while(c) {
			if (mem_only) {
				c->flags |= FL_MEM;
			} else {
				c->flags &= ~FL_MEM;
			}
			c = c->next;
		}
	} else {
		r = NULL;
	}

	if (ul.delete_urecord(_d, _a, r) < 0) {
		LM_ERR("failed to remove record from usrloc\n");

		/* Delete failed, try to get corresponding
		 * record structure and send back all existing
		 * contacts
		 */
		rerrno = R_UL_DEL_R;
		if (!ul.get_urecord(_d, _a, &r)) {
			build_contact(_m, r->contacts, _h);
			ul.release_urecord(r);
		}
		ul.unlock_udomain(_d, _a);
		return -1;
	}
	ul.unlock_udomain(_d, _a);
	return 0;
}


/*! \brief
*/
static struct socket_info *get_sock_val(struct sip_msg *msg)
{
	struct socket_info *sock;
	struct hdr_field *hf;
	str xsockname = str_init("socket");
	sr_xavp_t *vavp = NULL;
	str socks;
	str hosts;
	int port;
	int proto;
	char c = 0;

	if(sock_hdr_name.len>0) {
		if (parse_headers( msg, HDR_EOH_F, 0) == -1) {
			LM_ERR("failed to parse message\n");
			return 0;
		}

		for (hf=msg->headers; hf; hf=hf->next) {
			if (cmp_hdrname_str(&hf->name, &sock_hdr_name)==0)
				break;
		}

		/* hdr found? */
		if (hf==0)
			return 0;

		trim_len( socks.len, socks.s, hf->body );
		if (socks.len==0)
			return 0;

		/*FIXME: This is a hack */
		c = socks.s[socks.len];
		socks.s[socks.len] = '\0';
	} else {
		/* xavp */
		if(reg_xavp_cfg.s!=NULL)
			vavp = xavp_get_child_with_sval(&reg_xavp_cfg, &xsockname);
		if(vavp==NULL || vavp->val.v.s.len<=0)
			return 0;
		socks = vavp->val.v.s;
	}
	if (parse_phostport( socks.s, &hosts.s, &hosts.len,
				&port, &proto)!=0) {
		socks.s[socks.len] = c;
		LM_ERR("bad socket <%.*s> in \n",
				socks.len, socks.s);
		return 0;
	}
	if(sock_hdr_name.len>0 && c!=0) {
		socks.s[socks.len] = c;
	}
	sock = grep_sock_info(&hosts,(unsigned short)port,(unsigned short)proto);
	if (sock==0) {
		LM_ERR("non-local socket <%.*s>\n",	socks.len, socks.s);
		return 0;
	}

	LM_DBG("%d:<%.*s>:%d -> p=%p\n", proto,socks.len,socks.s,port_no,sock );

	return sock;
}



/*! \brief
 * Process request that contained no contact header
 * field, it means that we have to send back a response
 * containing a list of all existing bindings for the
 * given username (in To HF)
 */
static inline int no_contacts(sip_msg_t *_m, udomain_t* _d, str* _a, str* _h)
{
	urecord_t* r;
	int res;

	ul.lock_udomain(_d, _a);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LM_ERR("failed to retrieve record from usrloc\n");
		ul.unlock_udomain(_d, _a);
		return -1;
	}

	if (res == 0) {  /* Contacts found */
		build_contact(_m, r->contacts, _h);
		ul.release_urecord(r);
	} else {  /* No contacts found */
		build_contact(_m, NULL, _h);
	}
	ul.unlock_udomain(_d, _a);
	return 0;
}



/*! \brief
 * Fills the common part (for all contacts) of the info structure
 */
static inline ucontact_info_t* pack_ci( struct sip_msg* _m, contact_t* _c,
		unsigned int _e, unsigned int _f, int _use_regid)
{
	static ucontact_info_t ci;
	static str no_ua = str_init("n/a");
	static str callid;
	static str path_received = {0,0};
	static str path;
	static str received = {0,0};
	static int received_found;
	static unsigned int allowed, allow_parsed;
	static struct sip_msg *m = 0;
	int_str val;

	if (_m!=0) {
		memset( &ci, 0, sizeof(ucontact_info_t));

		/* Get callid of the message */
		callid = _m->callid->body;
		trim_trailing(&callid);
		if (callid.len > CALLID_MAX_SIZE) {
			rerrno = R_CALLID_LEN;
			LM_ERR("callid too long\n");
			goto error;
		}
		ci.callid = &callid;

		/* Get CSeq number of the message */
		if (str2int(&get_cseq(_m)->number, (unsigned int*)&ci.cseq) < 0) {
			rerrno = R_INV_CSEQ;
			LM_ERR("failed to convert cseq number\n");
			goto error;
		}

		/* set received socket */
		if (_m->flags&sock_flag) {
			ci.sock = get_sock_val(_m);
			if (ci.sock==0)
				ci.sock = _m->rcv.bind_address;
		} else {
			ci.sock = _m->rcv.bind_address;
		}

		/* set tcp connection id */
		if (_m->rcv.proto==PROTO_TCP || _m->rcv.proto==PROTO_TLS
				|| _m->rcv.proto==PROTO_WS  || _m->rcv.proto==PROTO_WSS) {
			ci.tcpconn_id = _m->rcv.proto_reserved1;
		} else {
			ci.tcpconn_id = -1;
		}

		/* additional info from message */
		if (parse_headers(_m, HDR_USERAGENT_F, 0) != -1 && _m->user_agent &&
				_m->user_agent->body.len>0 && _m->user_agent->body.len<MAX_UA_SIZE) {
			ci.user_agent = &_m->user_agent->body;
		} else {
			ci.user_agent = &no_ua;
		}

		/* extract Path headers */
		if (path_enabled) {
			if (build_path_vector(_m, &path, &path_received) < 0) {
				rerrno = R_PARSE_PATH;
				goto error;
			}
			if (path.len && path.s) {
				ci.path = &path;
				if (path_mode != PATH_MODE_OFF) {
					/* save in msg too for reply */
					if (set_path_vector(_m, &path) < 0) {
						rerrno = R_PARSE_PATH;
						goto error;
					}
				}
			}
		}

		ci.last_modified = act_time;

		/* set flags */
		ci.flags  = _f;
		getbflagsval(0, &ci.cflags);

		/* get received */
		if (path_received.len && path_received.s) {
			ci.cflags |= ul.nat_flag;
			ci.received = path_received;
		}

		ci.server_id = server_id;
		if(_m->contact) {
			_c = (((contact_body_t*)_m->contact->parsed)->contacts);
			if(_c->instance!=NULL && _c->instance->body.len>0) {
				ci.instance = _c->instance->body;
				LM_DBG("set instance[%.*s]\n", ci.instance.len, ci.instance.s);
			}
			if(_use_regid && _c->instance!=NULL && _c->reg_id!=NULL && _c->reg_id->body.len>0) {
				if(str2int(&_c->reg_id->body, &ci.reg_id)<0 || ci.reg_id==0)
				{
					LM_ERR("invalid reg-id value\n");
					goto error;
				}
			}
		}

		allow_parsed = 0; /* not parsed yet */
		received_found = 0; /* not found yet */
		m = _m; /* remember the message */
	}
	else {
		memset( &ci.instance, 0, sizeof(str));
	}

	if(_c!=0) {
		/* hook uri address - should be more than 'sip:' chars */
		if(_c->uri.s!=NULL && _c->uri.len>4)
			ci.c = &_c->uri;

		/* Calculate q value of the contact */
		if (m && m->id == q_override_msg_id)
		{
			ci.q = q_override_value;
		}
		else if (calc_contact_q(_c->q, &ci.q) < 0) {
			rerrno = R_INV_Q;
			LM_ERR("failed to calculate q\n");
			goto error;
		}

		/* set expire time */
		ci.expires = _e;

		/* Get methods of contact */
		if (_c->methods) {
			if (parse_methods(&(_c->methods->body), &ci.methods) < 0) {
				rerrno = R_PARSE;
				LM_ERR("failed to parse contact methods\n");
				goto error;
			}
		} else {
			/* check on Allow hdr */
			if (allow_parsed == 0) {
				if (m && parse_allow( m ) != -1) {
					allowed = get_allow_methods(m);
				} else {
					allowed = ALL_METHODS;
				}
				allow_parsed = 1;
			}
			ci.methods = allowed;
		}

		/* get received */
		if (ci.received.len==0) {
			if (_c->received) {
				ci.received = _c->received->body;
			} else {
				if (received_found==0) {
					memset(&val, 0, sizeof(int_str));
					if (rcv_avp_name.n!=0
							&& search_first_avp(rcv_avp_type, rcv_avp_name, &val, 0)
							&& val.s.len > 0) {
						if (val.s.len>RECEIVED_MAX_SIZE) {
							rerrno = R_CONTACT_LEN;
							LM_ERR("received too long\n");
							goto error;
						}
						received = val.s;
					} else {
						received.s = 0;
						received.len = 0;
					}
					received_found = 1;
				}
				ci.received = received;
			}
		}
		if(_c->instance!=NULL && _c->instance->body.len>0)
			ci.instance = _c->instance->body;
		if(_use_regid && _c->instance!=NULL && _c->reg_id!=NULL && _c->reg_id->body.len>0) {
			if(str2int(&_c->reg_id->body, &ci.reg_id)<0 || ci.reg_id==0)
			{
				LM_ERR("invalid reg-id value\n");
				goto error;
			}
		}
		if(sruid_next(&_reg_sruid)<0)
			goto error;
		ci.ruid = _reg_sruid.uid;
		LM_DBG("generated ruid is: %.*s\n", ci.ruid.len, ci.ruid.s);
	}

	return &ci;
error:
	return 0;
}


int reg_get_crt_max_contacts(void)
{
	int n;
	sr_xavp_t *vavp=NULL;
	str vname = {"max_contacts", 12};

	n = 0;

	if(reg_xavp_cfg.s!=NULL)
	{
		vavp = xavp_get_child_with_ival(&reg_xavp_cfg, &vname);
		if(vavp!=NULL)
		{
			n = vavp->val.v.i;
			LM_DBG("using max contacts value from xavp: %d\n", n);
		}
	}

	if(vavp==NULL)
	{
		n = cfg_get(registrar, registrar_cfg, max_contacts);
	}

	return n;
}

/*! \brief
 * Message contained some contacts, but record with same address
 * of record was not found so we have to create a new record
 * and insert all contacts from the message that have expires
 * > 0
 */
static inline int insert_contacts(struct sip_msg* _m, udomain_t* _d, str* _a, int _use_regid)
{
	ucontact_info_t* ci;
	urecord_t* r = NULL;
	ucontact_t* c;
	contact_t* _c;
	unsigned int flags;
	int num, expires;
	int maxc;
#ifdef USE_TCP
	int e_max, tcp_check;
	struct sip_uri uri;
#endif
	sip_uri_t *u;

	u = parse_to_uri(_m);
	if(u==NULL)
		goto error;

	flags = mem_only;
#ifdef USE_TCP
	if ( (_m->flags&tcp_persistent_flag)
			&& (_m->rcv.proto==PROTO_TCP||_m->rcv.proto==PROTO_TLS
				||_m->rcv.proto==PROTO_WS||_m->rcv.proto==PROTO_WSS)) {
		e_max = 0;
		tcp_check = 1;
	} else {
		e_max = tcp_check = 0;
	}
#endif
	_c = get_first_contact(_m);
	maxc = reg_get_crt_max_contacts();
	for( num=0,r=0,ci=0 ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &expires);
		/* Skip contacts with zero expires */
		if (expires == 0)
			continue;


		if (maxc > 0 && num >= maxc) {
			LM_INFO("too many contacts (%d) for AOR <%.*s>\n", 
					num, _a->len, _a->s);
			rerrno = R_TOO_MANY;
			goto error;
		}
		num++;

		if (r==0) {
			if (ul.insert_urecord(_d, _a, &r) < 0) {
				rerrno = R_UL_NEW_R;
				LM_ERR("failed to insert new record structure\n");
				goto error;
			}
		}

		/* pack the contact_info */
		if ( (ci=pack_ci( (ci==0)?_m:0, _c, expires, flags, _use_regid))==0 ) {
			LM_ERR("failed to extract contact info\n");
			goto error;
		}

		/* hack to work with buggy clients having many contacts with same
		 * address in one REGISTER - increase CSeq to detect if there was
		 * one already added, then update */
		ci->cseq++;
		if ( r->contacts==0
				|| ul.get_ucontact_by_instance(r, &_c->uri, ci, &c) != 0) {
			ci->cseq--;
			if (ul.insert_ucontact( r, &_c->uri, ci, &c) < 0) {
				rerrno = R_UL_INS_C;
				LM_ERR("failed to insert contact\n");
				goto error;
			}
		} else {
			ci->cseq--;
			if (ul.update_ucontact( r, c, ci) < 0) {
				rerrno = R_UL_UPD_C;
				LM_ERR("failed to update contact\n");
				goto error;
			}
		}
#ifdef USE_TCP
		if (tcp_check) {
			/* parse contact uri to see if transport is TCP */
			if (parse_uri( _c->uri.s, _c->uri.len, &uri)<0) {
				LM_ERR("failed to parse contact <%.*s>\n", 
						_c->uri.len, _c->uri.s);
			} else if (uri.proto==PROTO_TCP || uri.proto==PROTO_TLS || uri.proto==PROTO_WS || uri.proto==PROTO_WSS) {
				if (e_max) {
					LM_WARN("multiple TCP contacts on single REGISTER\n");
					if (expires>e_max) e_max = expires;
				} else {
					e_max = expires;
				}
			}
		}
#endif
	}

	if (r) {
		if (r->contacts)
			build_contact(_m, r->contacts, &u->host);
		ul.release_urecord(r);
	} else { /* No contacts found */
		build_contact(_m, NULL, &u->host);
	}

#ifdef USE_TCP
	if ( tcp_check && e_max>0 ) {
		e_max -= act_time;
		/*FIXME: Do we want this in the sr core?*/
		/*force_tcp_conn_lifetime( &_m->rcv , e_max + 10 );*/
	}
#endif

	return 0;
error:
	if (r)
		ul.delete_urecord(_d, _a, r);
	return -1;
}


static int test_max_contacts(struct sip_msg* _m, urecord_t* _r, contact_t* _c,
		ucontact_info_t *ci, int mc)
{
	int num;
	int e;
	ucontact_t* ptr, *cont;
	int ret;

	num = 0;
	ptr = _r->contacts;
	while(ptr) {
		if (VALID_CONTACT(ptr, act_time)) {
			num++;
		}
		ptr = ptr->next;
	}
	LM_DBG("%d valid contacts\n", num);

	for( ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &e);

		ret = ul.get_ucontact_by_instance( _r, &_c->uri, ci, &cont);
		if (ret==-1) {
			LM_ERR("invalid cseq for aor <%.*s>\n",_r->aor.len,_r->aor.s);
			rerrno = R_INV_CSEQ;
			return -1;
		} else if (ret==-2) {
			continue;
		}
		if (ret > 0) {
			/* Contact not found */
			if (e != 0) num++;
		} else {
			if (e == 0) num--;
		}
	}

	LM_DBG("%d contacts after commit\n", num);
	if (num > mc) {
		LM_INFO("too many contacts for AOR <%.*s>\n", _r->aor.len, _r->aor.s);
		rerrno = R_TOO_MANY;
		return -1;
	}

	return 0;
}


/*! \brief
 * Message contained some contacts and appropriate
 * record was found, so we have to walk through
 * all contacts and do the following:
 * 1) If contact in usrloc doesn't exists and
 *    expires > 0, insert new contact
 * 2) If contact in usrloc exists and expires
 *    > 0, update the contact
 * 3) If contact in usrloc exists and expires
 *    == 0, delete contact
 */
static inline int update_contacts(struct sip_msg* _m, urecord_t* _r, int _mode, int _use_regid)
{
	ucontact_info_t *ci;
	ucontact_t *c, *ptr, *ptr0;
	int expires, ret, updated;
	unsigned int flags;
#ifdef USE_TCP
	int e_max, tcp_check;
	struct sip_uri uri;
#endif
	int rc;
	contact_t* _c;
	int maxc;

	/* mem flag */
	flags = mem_only;

	rc = 0;
	/* pack the contact_info */
	if ( (ci=pack_ci( _m, 0, 0, flags, _use_regid))==0 ) {
		LM_ERR("failed to initial pack contact info\n");
		goto error;
	}

	if (!_mode) {
		maxc = reg_get_crt_max_contacts();
		if(maxc>0) {
			_c = get_first_contact(_m);
			if(test_max_contacts(_m, _r, _c, ci, maxc) != 0)
				goto error;
		}
	}

#ifdef USE_TCP
	if ( (_m->flags&tcp_persistent_flag) &&
			(_m->rcv.proto==PROTO_TCP||_m->rcv.proto==PROTO_TLS||_m->rcv.proto==PROTO_WS||_m->rcv.proto==PROTO_WSS)) {
		e_max = -1;
		tcp_check = 1;
	} else {
		e_max = tcp_check = 0;
	}
#endif

	_c = get_first_contact(_m);
	updated=0;
	for( ; _c ; _c = get_next_contact(_c) ) {
		/* calculate expires */
		calc_contact_expires(_m, _c->expires, &expires);

		/* pack the contact info */
		if ( (ci=pack_ci( 0, _c, expires, 0, _use_regid))==0 ) {
			LM_ERR("failed to pack contact specific info\n");
			goto error;
		}

		/* search for the contact*/
		ret = ul.get_ucontact_by_instance( _r, &_c->uri, ci, &c);
		if (ret==-1) {
			LM_ERR("invalid cseq for aor <%.*s>\n",_r->aor.len,_r->aor.s);
			rerrno = R_INV_CSEQ;
			goto error;
		} else if (ret==-2) {
			if(expires!=0 && _mode)
				break;
			continue;
		}

		if ( ret > 0 ) {
			/* Contact not found -> expired? */
			if (expires==0)
				continue;

			if (ul.insert_ucontact( _r, &_c->uri, ci, &c) < 0) {
				rerrno = R_UL_INS_C;
				LM_ERR("failed to insert contact\n");
				goto error;
			}
			rc = 1;
			if(_mode)
			{
				ptr=_r->contacts;
				while(ptr)
				{
					ptr0 = ptr->next;
					if(ptr!=c)
						ul.delete_ucontact(_r, ptr);
					ptr=ptr0;
				}
				updated=1;
			}
		} else {
			/* Contact found */
			if (expires == 0) {
				/* it's expired */
				if (mem_only) {
					c->flags |= FL_MEM;
				} else {
					c->flags &= ~FL_MEM;
				}

				if (ul.delete_ucontact(_r, c) < 0) {
					rerrno = R_UL_DEL_C;
					LM_ERR("failed to delete contact\n");
					goto error;
				}
				rc = 3;
			} else {
				/* do update */
				if(_mode)
				{
					ptr=_r->contacts;
					while(ptr)
					{
						ptr0 = ptr->next;
						if(ptr!=c)
							ul.delete_ucontact(_r, ptr);
						ptr=ptr0;
					}
					updated=1;
				}
				/* If call-id has changed then delete all records with this sip.instance
				   then insert new record */
				if (ci->instance.s != NULL &&
						(ci->callid->len != c->callid.len ||
						 strncmp(ci->callid->s, c->callid.s, ci->callid->len) != 0))
				{
					ptr = _r->contacts;
					while (ptr)
					{
						ptr0 = ptr->next;
						if ((ptr != c) && ptr->instance.len == c->instance.len &&
								strncmp(ptr->instance.s, c->instance.s, ptr->instance.len) == 0)
						{
							ul.delete_ucontact(_r, ptr);
						}
						ptr = ptr0;
					}
					updated = 1;
				}
				if (ul.update_ucontact(_r, c, ci) < 0) {
					rerrno = R_UL_UPD_C;
					LM_ERR("failed to update contact\n");
					goto error;
				}
				rc = 2;
			}
		}
#ifdef USE_TCP
		if (tcp_check) {
			/* parse contact uri to see if transport is TCP */
			if (parse_uri( _c->uri.s, _c->uri.len, &uri)<0) {
				LM_ERR("failed to parse contact <%.*s>\n", 
						_c->uri.len, _c->uri.s);
			} else if (uri.proto==PROTO_TCP || uri.proto==PROTO_TLS || uri.proto==PROTO_WS || uri.proto==PROTO_WSS) {
				if (e_max>0) {
					LM_WARN("multiple TCP contacts on single REGISTER\n");
				}
				if (expires>e_max) e_max = expires;
			}
		}
#endif
		/* have one contact only -- break */
		if(updated)
			break;
	}

#ifdef USE_TCP
	if ( tcp_check && e_max>-1 ) {
		if (e_max) e_max -= act_time;
		/*FIXME: Do we want this in the sr core? */
		/*force_tcp_conn_lifetime( &_m->rcv , e_max + 10 );*/
	}
#endif

	return rc;
error:
	return -1;
}


/*! \brief
 * This function will process request that
 * contained some contact header fields
 */
static inline int add_contacts(struct sip_msg* _m, udomain_t* _d,
		str* _a, int _mode, int _use_regid)
{
	int res;
	int ret;
	urecord_t* r;
	sip_uri_t *u;

	u = parse_to_uri(_m);
	if(u==NULL)
		return -2;

	ret = 0;
	ul.lock_udomain(_d, _a);
	res = ul.get_urecord(_d, _a, &r);
	if (res < 0) {
		rerrno = R_UL_GET_R;
		LM_ERR("failed to retrieve record from usrloc\n");
		ul.unlock_udomain(_d, _a);
		return -2;
	}

	if (res == 0) { /* Contacts found */
		if ((ret=update_contacts(_m, r, _mode, _use_regid)) < 0) {
			build_contact(_m, r->contacts, &u->host);
			ul.release_urecord(r);
			ul.unlock_udomain(_d, _a);
			return -3;
		}
		build_contact(_m, r->contacts, &u->host);
		ul.release_urecord(r);
	} else {
		if (insert_contacts(_m, _d, _a, _use_regid) < 0) {
			ul.unlock_udomain(_d, _a);
			return -4;
		}
		ret = 1;
	}
	ul.unlock_udomain(_d, _a);
	return ret;
}


/*!\brief
 * Process REGISTER request and save it's contacts
 */
#define is_cflag_set(_name) (((unsigned int)_cflags)&(_name))
int save(struct sip_msg* _m, udomain_t* _d, int _cflags, str *_uri)
{
	contact_t* c;
	int st, mode;
	str aor;
	int ret;
	sip_uri_t *u;
	rr_t *route;
	struct sip_uri puri;
	param_hooks_t hooks;
	param_t *params;
	contact_t *contact;
	int use_ob = 1, use_regid = 1;

	u = parse_to_uri(_m);
	if(u==NULL)
		goto error;

	rerrno = R_FINE;
	ret = 1;

	if (parse_message(_m) < 0) {
		goto error;
	}

	if (check_contacts(_m, &st) > 0) {
		goto error;
	}

	if (parse_supported(_m) == 0) {
		if (!(get_supported(_m)	& F_OPTION_TAG_OUTBOUND)
				&& reg_outbound_mode == REG_OUTBOUND_REQUIRE) {
			LM_WARN("Outbound required by server and not supported by UAC\n");
			rerrno = R_OB_UNSUP;
			goto error;
		}
	}

	if (parse_require(_m) == 0) {
		if ((get_require(_m) & F_OPTION_TAG_OUTBOUND)
				&& reg_outbound_mode == REG_OUTBOUND_NONE) {
			LM_WARN("Outbound required by UAC and not supported by server\n");
			rerrno = R_OB_REQD;
			goto error;
		}
	}

	if (reg_outbound_mode != REG_OUTBOUND_NONE
			&& _m->contact && _m->contact->parsed
			&& !(parse_headers(_m, HDR_VIA2_F, 0) == -1 || _m->via2 == 0
				|| _m->via2->error != PARSE_OK)) {
		/* Outbound supported on server, and more than one Via: - not the first hop */

		if (!(parse_headers(_m, HDR_PATH_F, 0) == -1 || _m->path == 0)) {
			route = (rr_t *)0;
			if (parse_rr_body(_m->path->body.s, _m->path->body.len, &route) < 0) {
				LM_ERR("Failed to parse Path: header body\n");
				goto error;
			}
			if (parse_uri(route->nameaddr.uri.s, route->nameaddr.uri.len, &puri) < 0) {
				LM_ERR("Failed to parse Path: URI\n");
				free_rr(&route);
				goto error;
			}
			if (parse_params(&puri.params, CLASS_URI, &hooks, &params) != 0) {
				LM_ERR("Failed to parse Path: URI parameters\n");
				free_rr(&route);
				goto error;
			}
			/* Not interested in param body - just the hooks */
			free_params(params);
			if (!hooks.uri.ob) {
				/* No ;ob parameter to top Path: URI - no outbound */
				use_ob = 0;
			}
			free_rr(&route);
		} else {
			/* No Path: header - no outbound */
			use_ob = 0;

		}

		contact = ((contact_body_t *) _m->contact->parsed)->contacts;
		if (!contact) {
			LM_ERR("empty Contact:\n");
			goto error;
		}

		if ((use_ob == 0) && (reg_regid_mode == REG_REGID_OUTBOUND)) {
			if ((get_supported(_m) & F_OPTION_TAG_OUTBOUND)
					&& contact->reg_id) {
				LM_WARN("Outbound used by UAC but not supported by edge proxy\n");
				rerrno = R_OB_UNSUP_EDGE;
				goto error;
			} else {
				/* ignore ;reg-id parameter */
				use_regid = 0;
			}
		}
	}

	get_act_time();
	c = get_first_contact(_m);

	if (extract_aor((_uri)?_uri:&get_to(_m)->uri, &aor, NULL) < 0) {
		LM_ERR("failed to extract Address Of Record\n");
		goto error;
	}

	mem_only = is_cflag_set(REG_SAVE_MEM_FL)?FL_MEM:FL_NONE;

	if (c == 0) {
		if (st) {
			if (star(_m, (udomain_t*)_d, &aor, &u->host) < 0) goto error;
			else ret=3;
		} else {
			if (no_contacts(_m, (udomain_t*)_d, &aor, &u->host) < 0) goto error;
			else ret=4;
		}
	} else {
		mode = is_cflag_set(REG_SAVE_REPL_FL)?1:0;
		if ((ret=add_contacts(_m, (udomain_t*)_d, &aor, mode, use_regid)) < 0)
			goto error;
		ret = (ret==0)?1:ret;
	}

	update_stat(accepted_registrations, 1);

	/* Only send reply upon request, not upon reply */
	if ((is_route_type(REQUEST_ROUTE) || is_route_type(FAILURE_ROUTE))
			&& !is_cflag_set(REG_SAVE_NORPL_FL) && (reg_send_reply(_m) < 0))
		return -1;

	if (path_enabled && path_mode != PATH_MODE_OFF) {
		reset_path_vector(_m);
	}
	return ret;
error:
	update_stat(rejected_registrations, 1);
	if (is_route_type(REQUEST_ROUTE) && !is_cflag_set(REG_SAVE_NORPL_FL) )
		reg_send_reply(_m);

	return 0;
}

int unregister(struct sip_msg* _m, udomain_t* _d, str* _uri, str *_ruid)
{
	str aor = {0, 0};
	sip_uri_t *u;
	urecord_t *r;
	ucontact_t *c;
	int res;

	if (_ruid == NULL) {
		/* No ruid provided - remove all contacts for aor */

		if (extract_aor(_uri, &aor, NULL) < 0) {
			LM_ERR("failed to extract Address Of Record\n");
			return -1;
		}

		u = parse_to_uri(_m);
		if(u==NULL)
			return -2;

		if (star(_m, _d, &aor, &u->host) < 0)
		{
			LM_ERR("error unregistering user [%.*s]\n", aor.len, aor.s);
			return -1;
		}
	} else {
		/* ruid provided - remove a specific contact */

		if (_uri->len > 0) {

			if (extract_aor(_uri, &aor, NULL) < 0) {
				LM_ERR("failed to extract Address Of Record\n");
				return -1;
			}

			if (ul.get_urecord_by_ruid(_d, ul.get_aorhash(&aor),
						_ruid, &r, &c) != 0) {
				LM_WARN("AOR/Contact not found\n");
				return -1;
			}
			if (ul.delete_ucontact(r, c) != 0) {
				ul.unlock_udomain(_d, &aor);
				LM_WARN("could not delete contact\n");
				return -1;
			}
			ul.unlock_udomain(_d, &aor);

		} else {

			res = ul.delete_urecord_by_ruid(_d, _ruid);
			switch (res) {
				case -1:
					LM_ERR("could not delete contact\n");
					return -1;
				case -2:
					LM_WARN("contact not found\n");
					return -1;
				default:
					return 1;
			}

		}
	}

	return 1;
}

int set_q_override(struct sip_msg* _m, int _q)
{
	if ((_q < 0) || (_q > 1000))
	{
		LM_ERR("Invalid q value\n");
		return -1;
	}
	q_override_msg_id = _m->id;
	q_override_value = _q;
	return 1;
}


