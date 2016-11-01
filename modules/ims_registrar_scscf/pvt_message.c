/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "pvt_message.h"
#include "../../modules/tm/tm_load.h"

extern struct tm_binds tmb;
extern struct _pv_req_data _pv_treq;

void pv_tmx_data_init(void) {
    memset(&_pv_treq, 0, sizeof (struct _pv_req_data));
}

int pv_t_copy_msg(struct sip_msg *src, struct sip_msg *dst) {
    dst->id = src->id;
    dst->rcv = src->rcv;
    dst->set_global_address = src->set_global_address;
    dst->set_global_port = src->set_global_port;
    dst->flags = src->flags;
    dst->fwd_send_flags = src->fwd_send_flags;
    dst->rpl_send_flags = src->rpl_send_flags;
    dst->force_send_socket = src->force_send_socket;

    if (parse_msg(dst->buf, dst->len, dst) != 0) {
        LM_ERR("parse msg failed\n");
        return -1;
    }
    return 0;
}

struct sip_msg* get_request_from_tx(struct cell *t) {
    if (t == NULL) {
        t = tmb.t_gett();
    }
    if (!t || t == (void*) - 1) {
        LM_ERR("Reply without transaction\n");
        return 0;
    }
    if (t) {

        /*  we may need the request message from here on.. if there are headers we need that were not parsed in the original request
        (which we cannot assume) then we would pollute the shm_msg t->uas.request if we did any parsing on it. Instead, we need to 
        make a private copy of the message and free it when we are done 
         */
	if (_pv_treq.label != t->label || _pv_treq.index != t->hash_index) {		
            /* make a copy */
            if (_pv_treq.buf == NULL || _pv_treq.buf_size < t->uas.request->len + 1) {
                if (_pv_treq.buf != NULL)
                    pkg_free(_pv_treq.buf);
                if (_pv_treq.tmsgp)
                    free_sip_msg(&_pv_treq.msg);
                _pv_treq.tmsgp = NULL;
                _pv_treq.index = 0;
                _pv_treq.label = 0;
                _pv_treq.buf_size = t->uas.request->len + 1;
                _pv_treq.buf = (char*) pkg_malloc(_pv_treq.buf_size * sizeof (char));
                if (_pv_treq.buf == NULL) {
                    LM_ERR("no more pkg\n");
                    _pv_treq.buf_size = 0;
                    return 0;
                }
            }
            if (_pv_treq.tmsgp)
                free_sip_msg(&_pv_treq.msg);
            memset(&_pv_treq.msg, 0, sizeof (struct sip_msg));
            memcpy(_pv_treq.buf, t->uas.request->buf, t->uas.request->len);
            _pv_treq.buf[t->uas.request->len] = '\0';
            _pv_treq.msg.len = t->uas.request->len;
            _pv_treq.msg.buf = _pv_treq.buf;
            _pv_treq.tmsgp = t->uas.request;
            _pv_treq.index = t->hash_index;
            _pv_treq.label = t->label;


            if (pv_t_copy_msg(t->uas.request, &_pv_treq.msg) != 0) {
                pkg_free(_pv_treq.buf);
                _pv_treq.buf_size = 0;
                _pv_treq.buf = NULL;
                _pv_treq.tmsgp = NULL;
                _pv_treq.index = 0;
                _pv_treq.label = 0;
                return 0;
            }
        }

        return &_pv_treq.msg;
    } else
        return 0;

}
