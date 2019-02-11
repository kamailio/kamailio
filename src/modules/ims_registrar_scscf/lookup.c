/*
 * $Id$
 *
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
 * History:
 * ---------
 * 2003-03-12 added support for zombie state (nils)
 */
/*!
 * \file
 * \brief SIP registrar module - lookup contacts in usrloc
 * \ingroup registrar
 */


#include <string.h>
#include "../../core/ut.h"
#include "../../core/dset.h"
#include "../../core/str.h"
#include "../../core/config.h"
#include "../../core/action.h"
#include "../../core/parser/parse_rr.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../lib/ims/ims_getters.h"
#include "common.h"
#include "regtime.h"
#include "ims_registrar_scscf_mod.h"
#include "lookup.h"
#include "config.h"
#include "save.h"
#include "pvt_message.h"

#define allowed_method(_msg, _c) \
	( !method_filtering || ((_msg)->REQ_METHOD)&((_c)->methods) )

/*! \brief
 * Lookup contact in the database and rewrite Request-URI
 * \return:  1 : found
 *          -1 : not found
 *          -2 : found but method not allowed
 *          -3 : error
 */
int lookup(struct sip_msg* _m, udomain_t* _d, char* ue_type_c) {
    impurecord_t* r;
    str aor;
    ucontact_t* ptr = 0;
    int res;
    int ret;
    str path_dst;
    flag_t old_bflags;
    int i = 0;
    int ue_type;    /*0=any, 1=3gpp, 2=sip */
	impu_contact_t *impucontact;

    if (!_m) {
        LM_ERR("NULL message!!!\n");
        return -1;
    }

    switch (ue_type_c[0]) {
            case '3':
                LM_DBG("looking for 3gpp terminals\n");
                ue_type = 1;
                break;
            case 's':
            case 'S':
                LM_DBG("looking for sip terminals\n");
                ue_type = 2;
                break;
        default:
            LM_DBG("looking for any type of terminal\n");
            ue_type=0;
    }

    if (_m->new_uri.s) {
			aor.s = pkg_malloc(_m->new_uri.len);
			if (aor.s == NULL) {
				LM_ERR("memory allocation failure\n");
				return -1;
			}
			memcpy(aor.s, _m->new_uri.s, _m->new_uri.len);
			aor.len = _m->new_uri.len;
		} else {
			aor.s = pkg_malloc(_m->first_line.u.request.uri.len);
			if (aor.s == NULL) {
				LM_ERR("memory allocation failure\n");
				return -1;
			}
			memcpy(aor.s, _m->first_line.u.request.uri.s, _m->first_line.u.request.uri.len);
			aor.len = _m->first_line.u.request.uri.len;
		}

    for (i = 4; i < aor.len; i++)
        if (aor.s[i] == ':' || aor.s[i] == ';' || aor.s[i] == '?') {
            aor.len = i;
            break;
        }

    LM_DBG("Looking for <%.*s>\n", aor.len, aor.s);

    get_act_time();

    ul.lock_udomain(_d, &aor);
    res = ul.get_impurecord(_d, &aor, &r);
    if (res != 0) {
        LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
				pkg_free(aor.s);
        ul.unlock_udomain(_d, &aor);
        return -1;
    }
    ret = -1;

	impucontact = r->linked_contacts.head;
    while (impucontact && (ptr = impucontact->contact)) {
        if (VALID_UE_TYPE(ptr, ue_type) && VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
            LM_DBG("Found a valid contact [%.*s]\n", ptr->c.len, ptr->c.s);
            break;
        }
		impucontact = impucontact->next;
    }

    /* look first for an un-expired and supported contact */
    if (ptr == 0) {
        LM_INFO("No contacts founds for IMPU <%.*s>\n", aor.len, aor.s);
        /* nothing found */
        goto done;
    }

    ret = 1;
    if (ptr) {
        if (rewrite_uri(_m, &ptr->c) < 0) {
            LM_ERR("unable to rewrite Request-URI\n");
            ret = -3;
            goto done;
        }

        /* reset next hop address */
        reset_dst_uri(_m);

        /* If a Path is present, use first path-uri in favour of
         * received-uri because in that case the last hop towards the uac
         * has to handle NAT. - agranig */
        if (ptr->path.s && ptr->path.len) {
            if (get_path_dst_uri(&ptr->path, &path_dst) < 0) {
                LM_ERR("failed to get dst_uri for Path\n");
                ret = -3;
                goto done;
            }
            if (set_path_vector(_m, &ptr->path) < 0) {
                LM_ERR("failed to set path vector\n");
                ret = -1;
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

        set_ruri_q(ptr->q);

        old_bflags = 0;
        getbflagsval(0, &old_bflags);
        setbflagsval(0, old_bflags | ptr->cflags);

        if (ptr->sock)
            set_force_socket(_m, ptr->sock);

        ptr = ptr->next;
    }

    /* Append branches if enabled */
    if (!cfg_get(registrar, registrar_cfg, append_branches)) goto done;

    //the last i was the first valid contact we found - let's go through the rest of valid contacts and append the branches.
    if (impucontact) impucontact = impucontact->next;
	while (impucontact) {
		ptr = impucontact->contact;
        if (VALID_UE_TYPE(ptr, ue_type) && VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
            path_dst.len = 0;
            if (ptr->path.s && ptr->path.len
                    && get_path_dst_uri(&ptr->path, &path_dst) < 0) {
                LM_ERR("failed to get dst_uri for Path\n");
                continue;
            }

            /* The same as for the first contact applies for branches
             * regarding path vs. received. */
            if (km_append_branch(_m, &ptr->c, path_dst.len ? &path_dst : &ptr->received,
                    &ptr->path, ptr->q, ptr->cflags, ptr->sock) == -1) {
                LM_ERR("failed to append a branch\n");
                /* Also give a chance to the next branches*/
                continue;
            }
        }
        impucontact = impucontact->next;
    }

done:
		pkg_free(aor.s);
    ul.unlock_udomain(_d, &aor);
    return ret;
}

int lookup_path_to_contact(struct sip_msg* _m, char* contact_uri) {
    ucontact_t* contact;
    str s_contact_uri;
    str path_dst;

    get_act_time();
    if (get_str_fparam(&s_contact_uri, _m, (fparam_t*) contact_uri) < 0) {
        LM_ERR("failed to get RURI\n");
        return -1;
    }
    LM_DBG("Looking up contact [%.*s]\n", s_contact_uri.len, s_contact_uri.s);

    if (ul.get_ucontact(&s_contact_uri, 0, 0, 0, &contact) == 0) { //get_contact returns with lock

        if (!VALID_CONTACT(contact, act_time)) {
            LM_DBG("Contact is not valid...ignoring\n");
            ul.release_ucontact(contact);
        } else {
            LM_DBG("CONTACT FOUND and path is [%.*s]\n", contact->path.len, contact->path.s);

            if (get_path_dst_uri(&contact->path, &path_dst) < 0) {
                LM_ERR("failed to get dst_uri for Path\n");
                ul.release_ucontact(contact);
                return -1;
            }
            if (set_path_vector(_m, &contact->path) < 0) {
                LM_ERR("failed to set path vector\n");
                ul.release_ucontact(contact);
                return -1;
            }
            if (set_dst_uri(_m, &path_dst) < 0) {
                LM_ERR("failed to set dst_uri of Path\n");
                ul.release_ucontact(contact);
                return -1;
            }

            ul.release_ucontact(contact);
            return 1;
        }
    }
    LM_DBG("no contact found for [%.*s]\n", s_contact_uri.len, s_contact_uri.s);
    return -1;
}

/*! \brief the impu_registered() function
 * Return true if the AOR in the To Header is registered
 */
int impu_registered(struct sip_msg* _m, char* _t, char* _s) {
    impurecord_t* r;
    int res, ret = -1;

    str impu;
    impu = cscf_get_public_identity(_m);

    LM_DBG("Looking for IMPU <%.*s>\n", impu.len, impu.s);

    ul.lock_udomain((udomain_t*) _t, &impu);
    res = ul.get_impurecord((udomain_t*) _t, &impu, &r);

    if (res < 0) {
        ul.unlock_udomain((udomain_t*) _t, &impu);
        LM_ERR("failed to query usrloc for IMPU <%.*s>\n", impu.len, impu.s);
        return ret;
    }

    if (res == 0) {
        if (r->reg_state == IMPU_REGISTERED) ret = 1;
        ul.unlock_udomain((udomain_t*) _t, &impu);
        LM_DBG("'%.*s' found in usrloc\n", impu.len, ZSW(impu.s));
        return ret;
    }

    ul.unlock_udomain((udomain_t*) _t, &impu);
    LM_DBG("'%.*s' not found in usrloc\n", impu.len, ZSW(impu.s));
    return ret;
}

/**
 * Check that the IMPU at the Term S has at least one valid contact...
 * @param _m - msg
 * @param _t - t
 * @param _s - s
 * @return true if there is at least one valid contact. false if not
 */
int term_impu_has_contact(struct sip_msg* _m, udomain_t* _d, char* _s) {
    impurecord_t* r;
    str aor, uri;
    ucontact_t* ptr = 0;
    int res;
    int ret;

	impu_contact_t *impucontact;

    if (_m->new_uri.s) uri = _m->new_uri;
    else uri = _m->first_line.u.request.uri;

    if (extract_aor(&uri, &aor) < 0) {
        LM_ERR("failed to extract address of record\n");
        return -3;
    }

    get_act_time();

    ul.lock_udomain(_d, &aor);
    res = ul.get_impurecord(_d, &aor, &r);
    if (res != 0) {
        LM_DBG("'%.*s' Not found in usrloc\n", aor.len, ZSW(aor.s));
        ul.unlock_udomain(_d, &aor);
        return -1;
    }

    impucontact = r->linked_contacts.head;
	while (impucontact) {
		ptr = impucontact->contact;
        if (VALID_CONTACT(ptr, act_time) && allowed_method(_m, ptr)) {
            LM_DBG("Found a valid contact [%.*s]\n", ptr->c.len, ptr->c.s);
            ret = 1;
            break;
        }
		impucontact = impucontact->next;
    }

    /* look first for an un-expired and supported contact */
    if (ptr == 0) {
        /* nothing found */
        ret = -1;
    }

    ul.unlock_udomain(_d, &aor);

    return ret;
}

/*! \brief the term_impu_registered() function
 * Return true if the AOR in the Request-URI  for the terminating user is registered
 */
int term_impu_registered(struct sip_msg* _m, char* _t, char* _s) {
    struct sip_msg *req;
    int i;
    str uri;
    impurecord_t* r;
    int res;

    //	if (_m->new_uri.s) uri = _m->new_uri;
    //	else uri = _m->first_line.u.request.uri;
    //
    //	if (extract_aor(&uri, &aor) < 0) {
    //		LM_ERR("failed to extract address of record\n");
    //		return -1;
    //	}

    req = _m;
    if (!req) {
        LM_ERR(":term_impu_registered: NULL message!!!\n");
        return -1;
    }
    if (req->first_line.type != SIP_REQUEST) {
        req = get_request_from_tx(0);
    }

    if (_m->new_uri.s) uri = _m->new_uri;
    else uri = _m->first_line.u.request.uri;

    for (i = 0; i < uri.len; i++)
        if (uri.s[i] == ';' || uri.s[i] == '?' || (i > 3 /*sip:*/ && uri.s[i] == ':' /*strip port*/)) {
            uri.len = i;
            break;
        }
    LM_DBG("term_impu_registered: Looking for <%.*s>\n", uri.len, uri.s);

    ul.lock_udomain((udomain_t*) _t, &uri);
    res = ul.get_impurecord((udomain_t*) _t, &uri, &r);

    if (res != 0) {
        ul.unlock_udomain((udomain_t*) _t, &uri);
        LM_DBG("failed to query for terminating IMPU or not found <%.*s>\n", uri.len, uri.s);
        return -1;
    }

    ul.unlock_udomain((udomain_t*) _t, &uri);
    LM_DBG("'%.*s' found in usrloc\n", uri.len, ZSW(uri.s));
    return 1;

}
