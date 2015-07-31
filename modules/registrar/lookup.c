/*
 * Lookup contacts in usrloc
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief SIP registrar module - lookup contacts in usrloc
 * \ingroup registrar  
 */  


#include <string.h>
#include "../../ut.h"
#include "../../dset.h"
#include "../../str.h"
#include "../../xavp.h"
#include "../../config.h"
#include "../../action.h"
#include "../../mod_fix.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_to.h"
#include "../../forward.h"
#include "../usrloc/usrloc.h"
#include "common.h"
#include "regtime.h"
#include "reg_mod.h"
#include "lookup.h"
#include "config.h"

#define allowed_method(_msg, _c) \
	( !method_filtering || ((_msg)->REQ_METHOD)&((_c)->methods) )


/**
 * compare two instances, by skipping '<' & '>'
 */
int reg_cmp_instances(str *i1, str *i2)
{
	str inst1;
	str inst2;

	if(i1==NULL || i2==NULL || i1->len<=0 || i2->len<=0)
		return -1;

	inst1 = *i1;
	inst2 = *i2;
	if(inst1.len>2 && inst1.s[0]=='<' && inst1.s[inst1.len-1]=='>')
	{
		inst1.s++;
		inst1.len -=2;
	}
	if(inst2.len>2 && inst2.s[0]=='<' && inst2.s[inst2.len-1]=='>')
	{
		inst2.s++;
		inst2.len -=2;
	}
	if(inst1.len>0 && inst1.len==inst2.len
						&& memcmp(inst1.s, inst2.s, inst2.len)==0)
		return 0;
	return -1;
}

/*! \brief
 * Lookup a contact in usrloc and rewrite R-URI if found
 */
int lookup(struct sip_msg* _m, udomain_t* _d, str* _uri) {
     return lookup_helper(_m, _d, _uri, 0);
}

/*! \brief
 * Lookup a contact in usrloc and add the records to the dset structure
 */
int lookup_to_dset(struct sip_msg* _m, udomain_t* _d, str* _uri) {
     return lookup_helper(_m, _d, _uri, 1);
}

/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * or not according to _mode value:
 *  0: rewrite
 *  1: don't rewrite
 * \return: -1 : not found
 *          -2 : found but method not allowed
 *          -3 : error
 */
int lookup_helper(struct sip_msg* _m, udomain_t* _d, str* _uri, int _mode)
{
	urecord_t* r;
	str aor, uri;
	sip_uri_t puri;
	ucontact_t* ptr = 0;
	int res;
	int ret;
	str path_dst;
	flag_t old_bflags;
	int i;
	str inst = {0};
	unsigned int ahash = 0;
	sr_xavp_t *xavp=NULL;
	sr_xavp_t *list=NULL;
	str xname = {"ruid", 4};
	sr_xval_t xval;
	sip_uri_t path_uri;
	str path_str;

	ret = -1;

	if (_m->new_uri.s) uri = _m->new_uri;
	else uri = _m->first_line.u.request.uri;
	
	if (extract_aor((_uri)?_uri:&uri, &aor, &puri) < 0) {
		LM_ERR("failed to extract address of record\n");
		return -3;
	}
	/* check if gruu */
	if(puri.gr.s!=NULL)
	{
		if(puri.gr_val.len>0) {
			/* pub-gruu */
			inst = puri.gr_val;
			LM_DBG("looking up pub gruu [%.*s]\n", inst.len, inst.s);
		} else {
			/* temp-gruu */
			ahash = 0;
			inst = puri.user;
			for(i=inst.len-1; i>=0; i--)
			{
				if(inst.s[i]==REG_GRUU_SEP)
					break;
				ahash <<= 4;
				if(inst.s[i] >='0' && inst.s[i] <='9') ahash+=inst.s[i] -'0';
				else if (inst.s[i] >='a' && inst.s[i] <='f') ahash+=inst.s[i] -'a'+10;
				else if (inst.s[i] >='A' && inst.s[i] <='F') ahash+=inst.s[i] -'A'+10;
				else {
					LM_ERR("failed to extract temp gruu - invalid hash\n");
					return -3;
				}
			}
			if(i<0) {
				LM_ERR("failed to extract temp gruu - invalid format\n");
				return -3;
			}
			inst.len = i;
			LM_DBG("looking up temp gruu [%u / %.*s]\n", ahash, inst.len, inst.s);
		}
	}

	get_act_time();

	if(puri.gr.s==NULL || puri.gr_val.len>0)
	{
		/* aor or pub-gruu lookup */
		ul.lock_udomain(_d, &aor);
		res = ul.get_urecord(_d, &aor, &r);
		if (res > 0) {
			LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
			ul.unlock_udomain(_d, &aor);
			return -1;
		}

		ptr = r->contacts;
		ret = -1;
		/* look first for an un-expired and suported contact */
		while (ptr) {
			if(VALID_CONTACT(ptr,act_time)) {
				if(allowed_method(_m,ptr)) {
					/* match on instance, if pub-gruu */
					if(inst.len>0) {
						if(reg_cmp_instances(&inst, &ptr->instance)==0)
						{
							/* pub-gruu - found by instance */
							LM_DBG("contact for [%.*s] found by pub gruu [%.*s]\n",
								aor.len, ZSW(aor.s), inst.len, inst.s);
							break;
						}
					} else {
						/* no-gruu - found by address */
						LM_DBG("contact for [%.*s] found by address\n",
								aor.len, ZSW(aor.s));
						break;
					}
				} else {
					LM_DBG("contact for [%.*s] cannot handle the SIP method\n",
							aor.len, ZSW(aor.s));
					ret = -2;
				}
			}
			ptr = ptr->next;
		}
		if (ptr==0) {
			/* nothing found */
			LM_DBG("'%.*s' has no valid contact in usrloc\n", aor.len, ZSW(aor.s));
			goto done;
		}
	} else {
		/* temp-gruu lookup */
		res = ul.get_urecord_by_ruid(_d, ahash, &inst, &r, &ptr);
		if(res<0) {
			LM_DBG("temp gruu '%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
			return -1;
		}
		aor = *ptr->aor;
		/* test if un-expired and suported contact */
		if( (ptr) && !(VALID_CONTACT(ptr,act_time)
					&& (ret=-2) && allowed_method(_m,ptr)))
			goto done;
		LM_DBG("contact for [%.*s] found by temp gruu [%.*s / %u]\n",
							aor.len, ZSW(aor.s), inst.len, inst.s, ahash);
	}

	ret = 1;
	/* don't rewrite r-uri if called by lookup_to_dset */
	if (_mode == 0 && ptr) {
		if (rewrite_uri(_m, &ptr->c) < 0) {
			LM_ERR("unable to rewrite Request-URI\n");
			ret = -3;
			goto done;
		}

		/* reset next hop address */
		reset_dst_uri(_m);

		/* add xavp with details of the record (ruid, ...) */
		if(reg_xavp_rcd.s!=NULL)
		{
			list = xavp_get(&reg_xavp_rcd, NULL);
			xavp = list;
			memset(&xval, 0, sizeof(sr_xval_t));
			xval.type = SR_XTYPE_STR;
			xval.v.s = ptr->ruid;
			xavp_add_value(&xname, &xval, &xavp);
			if(list==NULL)
			{
				/* no reg_xavp_rcd xavp in root list - add it */
				xval.type = SR_XTYPE_XAVP;
				xval.v.xavp = xavp;
				xavp_add_value(&reg_xavp_rcd, &xval, NULL);
			}
		}

		/* If a Path is present, use first path-uri in favour of
		 * received-uri because in that case the last hop towards the uac
		 * has to handle NAT. - agranig */
		if (ptr->path.s && ptr->path.len) {
			/* make a copy, so any change we need to make here does not mess up the structure in usrloc */
			path_str = ptr->path;
			if (get_path_dst_uri(&path_str, &path_dst) < 0) {
				LM_ERR("failed to get dst_uri for Path\n");
				ret = -3;
				goto done;
			}
			if (path_check_local > 0) {
				if (parse_uri(path_dst.s, path_dst.len, &path_uri) < 0){
					LM_ERR("failed to parse the Path URI\n");
					ret = -3;
					goto done;
				}
				if (check_self(&(path_uri.host), 0, 0)) {
					/* first hop in path vector is local - check for additional hops and if present, point to next one */
					if (path_str.len > (path_dst.len + 3)) {
						path_str.s = path_str.s + path_dst.len + 3;
						path_str.len = path_str.len - path_dst.len - 3;
						if (get_path_dst_uri(&path_str, &path_dst) < 0) {
							LM_ERR("failed to get second dst_uri for Path\n");
							ret = -3;
							goto done;
						}
					} else {
						/* no more hops */
						path_dst.s = NULL;
						path_dst.len = 0;
					}
				}
			}
		} else {
			path_dst.s = NULL;
			path_dst.len = 0;
		}
		if (path_dst.s && path_dst.len) {
			if (set_path_vector(_m, &path_str) < 0) {
				LM_ERR("failed to set path vector\n");
				ret = -3;
				goto done;
			}
			if (set_dst_uri(_m, &path_dst) < 0) {
				LM_ERR("failed to set dst_uri of Path\n");
				ret = -3;
				goto done;
			}
		} else if (ptr->received.s && ptr->received.len) {
			if (set_dst_uri(_m, &ptr->received) < 0) {
				ret = -3;
				goto done;
			}
		}

		if (ptr->instance.len) {
		    if (set_instance(_m, &(ptr->instance)) < 0) {
				ret = -3;
				goto done;
		    }
		}
		
		_m->reg_id = ptr->reg_id;

		if (ptr->ruid.len) {
		    if (set_ruid(_m, &(ptr->ruid)) < 0) {
				ret = -3;
				goto done;
		    }
		}

		if (ptr->user_agent.len) {
		    if (set_ua(_m, &(ptr->user_agent)) < 0) {
				ret = -3;
				goto done;
		    }
		}

		set_ruri_q(ptr->q);

		old_bflags = 0;
		getbflagsval(0, &old_bflags);
		setbflagsval(0, old_bflags|ptr->cflags);

		if (ptr->sock)
			set_force_socket(_m, ptr->sock);

		if(ptr->xavp!=NULL) {
			xavp = xavp_clone_level_nodata(ptr->xavp);
			if(xavp_insert(xavp, 0, NULL)<0) {
				ret = -3;
				goto done;
			}
		}
		ptr = ptr->next;
	}

	/* if was gruu, no more branches */
	if(inst.len>0) goto done;

	/* Append branches if enabled */
	if (!cfg_get(registrar, registrar_cfg, append_branches)) goto done;

	for( ; ptr ; ptr = ptr->next ) {
		if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
			path_dst.len = 0;
			if(ptr->path.s && ptr->path.len) {
				path_str = ptr->path;
				if (get_path_dst_uri(&path_str, &path_dst) < 0) {
					LM_ERR("failed to get dst_uri for Path\n");
					continue;
				}
				if (path_check_local > 0) {
					if (parse_uri(path_dst.s, path_dst.len, &path_uri) < 0) {
						LM_ERR("failed to parse the Path URI\n");
						continue;
					}
					if (check_self(&(path_uri.host), 0, 0)) {
						/* first hop in path vector is local - check for additional hops and if present, point to next one */
						if (path_str.len > (path_dst.len + 3)) {
							path_str.s = path_str.s + path_dst.len + 3;
							path_str.len = path_str.len - path_dst.len - 3;
							if (get_path_dst_uri(&path_str, &path_dst) < 0) {
								LM_ERR("failed to get second dst_uri for Path\n");
								continue;
							}
						} else {
							/* no more hops */
							path_dst.s = NULL;
							path_dst.len = 0;
						}
					}
				}
			} else {
				path_dst.s = NULL;
				path_dst.len = 0;
			}

			/* The same as for the first contact applies for branches 
			 * regarding path vs. received. */
			LM_DBG("instance is %.*s\n",
			       ptr->instance.len, ptr->instance.s);
			if (append_branch(_m, &ptr->c,
					  path_dst.len?&path_dst:&ptr->received,
					  path_dst.len?&path_str:0, ptr->q, ptr->cflags,
					  ptr->sock,
					  ptr->instance.len?&(ptr->instance):0,
				          ptr->instance.len?ptr->reg_id:0,
					  &ptr->ruid, &ptr->user_agent)
			    == -1) {
				LM_ERR("failed to append a branch\n");
				/* Also give a chance to the next branches*/
				continue;
			}
			if(ptr->xavp!=NULL) {
				xavp = xavp_clone_level_nodata(ptr->xavp);
				if(xavp_insert(xavp, nr_branches, NULL)<0) {
					ret = -3;
					goto done;
				}
			}
		}
	}

done:
	ul.release_urecord(r);
	ul.unlock_udomain(_d, &aor);
	return ret;
}


/**
 * only reset the pointers after local backup in lookup_branches
 */
int clear_ruri_branch(sip_msg_t *msg)
{
	if(msg==NULL)
		return -1;

	msg->dst_uri.s = 0;
	msg->dst_uri.len = 0;
	msg->path_vec.s = 0;
	msg->path_vec.len = 0;
	set_ruri_q(Q_UNSPECIFIED);
	reset_force_socket(msg);
	setbflagsval(0, 0);
	msg->instance.len = 0;
	msg->reg_id = 0;
	msg->ruid.s = 0;
	msg->ruid.len = 0;
	msg->location_ua.s = 0;
	msg->location_ua.len = 0;
	return 0;
}

/**
 * reset and free the pointers after cloning to a branch in lookup_branches
 */
int reset_ruri_branch(sip_msg_t *msg)
{
    if(msg==NULL)
        return -1;

    reset_dst_uri(msg);
    reset_path_vector(msg);
    set_ruri_q(Q_UNSPECIFIED);
    reset_force_socket(msg);
    setbflagsval(0, 0);
    reset_instance(msg);
    msg->reg_id = 0;
    reset_ruid(msg);
    reset_ua(msg);
    return 0;
}

/*! \brief
 * Lookup contacts in the database for all branches, including R-URI
 * \return: -1 : not found
 *          -2 : found but method not allowed (for r-uri)
 *          -3 : error
 */
int lookup_branches(sip_msg_t *msg, udomain_t *d)
{
	unsigned int nr_branches_start;
	unsigned int i;
	int ret;
	int found;
	str new_uri;
	str ruri_b_uri = {0};
	str ruri_b_dst_uri = {0};
	str ruri_b_path = {0};
	int ruri_b_q = Q_UNSPECIFIED;
	struct socket_info *ruri_b_socket = 0;
	flag_t ruri_b_flags = 0;
	str ruri_b_instance = {0};
	unsigned int ruri_b_reg_id = 0;
	str ruri_b_ruid = {0};
	str ruri_b_ua = {0};
	branch_t *crt = NULL;

	ret = 1;
	found  = 0;
	nr_branches_start = nr_branches;
	/* first lookup the r-uri */
	ret = lookup(msg, d, NULL);

	/* if no other branches -- all done */
	if(nr_branches_start==0)
		return ret;

	if(ret>0)
		found = 1;

	/* backup r-uri branch */
	ruri_b_uri = msg->new_uri;
	ruri_b_dst_uri = msg->dst_uri;
	ruri_b_path = msg->path_vec;
	ruri_b_q = get_ruri_q();
	ruri_b_socket = msg->force_send_socket;
	getbflagsval(0, &ruri_b_flags);
	ruri_b_instance = msg->instance;
	ruri_b_reg_id = msg->reg_id;
	ruri_b_ruid = msg->ruid;
	ruri_b_ua = msg->location_ua;
	clear_ruri_branch(msg);
	/* set new uri buf to null, otherwise is freed or overwritten by
	 * rewrite_uri() during branch lookup */
	msg->new_uri.len=0;
	msg->new_uri.s=0;
	msg->parsed_uri_ok=0;

	for(i=0; i<nr_branches_start; i++) {
		crt = get_sip_branch(i);
		/* it has to be a clean branch to do lookup for it */
		if(crt->len <= 0 || crt->dst_uri_len > 0
				|| crt->path_len > 0 || crt->force_send_socket!=NULL
				|| crt->flags !=0)
			continue;
		/* set the new uri from branch and lookup */
		new_uri.s = crt->uri;
		new_uri.len = crt->len;
		if (rewrite_uri(msg, &new_uri) < 0) {
			LM_ERR("unable to rewrite Request-URI for branch %u\n", i);
			ret = -3;
			goto done;
		}
		ret = lookup(msg, d, NULL);
		if(ret>0) {
			/* move r-uri branch attributes to crt branch */
			found = 1;

			if (unlikely(msg->new_uri.len > MAX_URI_SIZE - 1)) {
				LM_ERR("too long uri: %.*s\n", msg->new_uri.len,
						msg->new_uri.s);
				ret = -3;
				goto done;
			}

			/* copy the dst_uri */
			if (msg->dst_uri.len>0 && msg->dst_uri.s!=NULL) {
				if (unlikely(msg->dst_uri.len > MAX_URI_SIZE - 1)) {
					LM_ERR("too long dst_uri: %.*s\n", msg->dst_uri.len,
							msg->dst_uri.s);
					ret = -3;
					goto done;
				}

				memcpy(crt->dst_uri, msg->dst_uri.s, msg->dst_uri.len);
				crt->dst_uri[msg->dst_uri.len] = 0;
				crt->dst_uri_len = msg->dst_uri.len;
			}

			/* copy the path string */
			if (unlikely(msg->path_vec.len>0 && msg->path_vec.s!=NULL)) {
				if (unlikely(msg->path_vec.len > MAX_PATH_SIZE - 1)) {
					LM_ERR("too long path: %.*s\n", msg->path_vec.len,
							msg->path_vec.s);
					ret = -3;
					goto done;
				}
				memcpy(crt->path, msg->path_vec.s, msg->path_vec.len);
				crt->path[msg->path_vec.len] = 0;
				crt->path_len = msg->path_vec.len;
			}

			/* copy the ruri */
			memcpy(crt->uri, msg->new_uri.s, msg->new_uri.len);
			crt->uri[msg->new_uri.len] = 0;
			crt->len = msg->new_uri.len;
			crt->q = get_ruri_q();

			crt->force_send_socket = msg->force_send_socket;
			getbflagsval(0, &crt->flags);
		}
		reset_ruri_branch(msg);
	}

done:
	reset_ruri_branch(msg);
	/* new uri could be set to allocated buffer by branch lookup */
	if(msg->new_uri.s!=NULL)
		pkg_free(msg->new_uri.s);
	msg->new_uri = ruri_b_uri;
	ruri_mark_new();
	msg->parsed_uri_ok = 0;
	msg->dst_uri = ruri_b_dst_uri;
	msg->path_vec = ruri_b_path;
	set_ruri_q(ruri_b_q);
	set_force_socket(msg, ruri_b_socket);
	setbflagsval(0, ruri_b_flags);
	msg->instance = ruri_b_instance;
	msg->reg_id = ruri_b_reg_id;
	msg->ruid = ruri_b_ruid;
	msg->location_ua = ruri_b_ua;

	return (found)?1:ret;
}

/*! \brief the is_registered() function
 * Return true if the AOR in the Request-URI is registered,
 * it is similar to lookup but registered neither rewrites
 * the Request-URI nor appends branches
 */
int registered(struct sip_msg* _m, udomain_t* _d, str* _uri) {
	return registered4(_m, _d, _uri, 0, 0);
}

int registered3(struct sip_msg* _m, udomain_t* _d, str* _uri, int match_flag) {
	return registered4(_m, _d, _uri, match_flag, 0);
}

int registered4(struct sip_msg* _m, udomain_t* _d, str* _uri, int match_flag, int match_action_flag)
{
	str uri, aor;
	urecord_t* r;
	ucontact_t* ptr;
	int res;
	str match_callid = {0,0};
	str match_received = {0,0};
	str match_contact = {0,0};
	sr_xavp_t *vavp = NULL;

	if(_uri!=NULL)
	{
		uri = *_uri;
	} else {
		if(IS_SIP_REPLY(_m)) {
			if (parse_to_header(_m) < 0) {
				LM_ERR("failed to prepare the message\n");
				return -1;
			}
			uri = get_to(_m)->uri;
		} else {
			if (_m->new_uri.s) uri = _m->new_uri;
			else uri = _m->first_line.u.request.uri;
		}
	}
	
	if (extract_aor(&uri, &aor, NULL) < 0) {
		LM_ERR("failed to extract address of record\n");
		return -1;
	}
	
	ul.lock_udomain(_d, &aor);
	res = ul.get_urecord(_d, &aor, &r);

	if (res < 0) {
		ul.unlock_udomain(_d, &aor);
		LM_ERR("failed to query usrloc\n");
		return -1;
	}

	if (res == 0) {
		LM_DBG("searching with match flags (%d,%d)\n", match_flag, match_action_flag);
		if(reg_xavp_cfg.s!=NULL) {

			if((match_flag & 1)
					&& (vavp = xavp_get_child_with_sval(&reg_xavp_cfg, &match_callid_name)) != NULL
					&& vavp->val.v.s.len > 0) {
				match_callid = vavp->val.v.s;
				LM_DBG("matching with callid %.*s\n", match_callid.len, match_callid.s);
			}

			if((match_flag & 2)
					&& (vavp = xavp_get_child_with_sval(&reg_xavp_cfg, &match_received_name)) != NULL
					&& vavp->val.v.s.len > 0) {
				match_received = vavp->val.v.s;
				LM_DBG("matching with received %.*s\n", match_received.len, match_received.s);
			}

			if((match_flag & 4)
					&& (vavp = xavp_get_child_with_sval(&reg_xavp_cfg, &match_contact_name)) != NULL
					&& vavp->val.v.s.len > 0) {
				match_contact = vavp->val.v.s;
				LM_DBG("matching with contact %.*s\n", match_contact.len, match_contact.s);
			}
		}

		for (ptr = r->contacts; ptr; ptr = ptr->next) {
			if(!VALID_CONTACT(ptr, act_time)) continue;
			if (match_callid.s && /* optionally enforce tighter matching w/ Call-ID */
				match_callid.len > 0 &&
				(match_callid.len != ptr->callid.len || 
				memcmp(match_callid.s, ptr->callid.s, match_callid.len)))
				continue;
			if (match_received.s && /* optionally enforce tighter matching w/ ip:port */
				match_received.len > 0 &&
				(match_received.len != ptr->received.len || 
				memcmp(match_received.s, ptr->received.s, match_received.len)))
				continue;
			if (match_contact.s && /* optionally enforce tighter matching w/ Contact */
				match_contact.len > 0 &&
				(match_contact.len != ptr->c.len || 
				memcmp(match_contact.s, ptr->c.s, match_contact.len)))
				continue;

			if(ptr->xavp!=NULL && match_action_flag == 1) {
				sr_xavp_t *xavp = xavp_clone_level_nodata(ptr->xavp);
				if(xavp_add(xavp, NULL)<0) {
					LM_ERR("error adding xavp for %.*s after successful match\n", aor.len, ZSW(aor.s));
					xavp_destroy_list(&xavp);
				}
			}

			ul.release_urecord(r);
			ul.unlock_udomain(_d, &aor);
			LM_DBG("'%.*s' found in usrloc\n", aor.len, ZSW(aor.s));

			return 1;
		}
	}

	ul.unlock_udomain(_d, &aor);
	LM_DBG("'%.*s' not found in usrloc\n", aor.len, ZSW(aor.s));
	return -1;
}
