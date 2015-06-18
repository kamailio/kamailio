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
 *

 *! \file
 *  \brief USRLOC - Usrloc contact handling functions
 *  \ingroup usrloc
 *
 * - Module: \ref usrloc
 */

#include "ucontact.h"
#include <string.h>             /* memcpy */
#include "../../mem/shm_mem.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db.h"
#include "ul_mod.h"
#include "ul_callback.h"
#include "usrloc.h"
#include "impurecord.h"
#include "ucontact.h"
#include "usrloc.h"
#include "usrloc_db.h"
#include "../../hashes.h"
#include "contact_hslot.h"
#include "utime.h"

extern struct contact_list* contact_list;
extern int db_mode;

/*!
 * \brief Create a new contact structure
 * \param _dom domain
 * \param _aor address of record
 * \param _contact contact string
 * \param _ci contact informations
 * \return new created contact on success, 0 on failure
 */
ucontact_t* new_ucontact(str* _dom, str* _aor, str* _contact, ucontact_info_t* _ci) {
    ucontact_t *c;
    param_t *prev, *curr, *param;
    int first = 1;

    c = (ucontact_t*) shm_malloc(sizeof (ucontact_t));
    if (!c) {
        LM_ERR("no more shm memory\n");
        return 0;
    }
    memset(c, 0, sizeof (ucontact_t));
    
    c->lock = lock_alloc();
    if (c->lock==0){
        goto error;
    }
    if (lock_init(c->lock)==0){
	lock_dealloc(c->lock);
	c->lock=0;
	goto error;
    }

    //setup callback list
    c->cbs = (struct ulcb_head_list*) shm_malloc(sizeof (struct ulcb_head_list));
    if (c->cbs == 0) {
        LM_CRIT("no more shared mem\n");
        goto error;
    }
    c->cbs->first = 0;
    c->cbs->reg_types = 0;
    
    /*Copy parameter list into shm**/
    param = _ci->params;
    while(param) {
	/*Copy first param in curr*/
	curr = shm_malloc(sizeof (param_t));
	curr->len = param->len;
	curr->type = param->type;
	curr->next = 0;
	if (shm_str_dup(&curr->body, &param->body) < 0) goto error;
	if (shm_str_dup(&curr->name, &param->name) < 0) goto error;
	
	if(first) {
	    c->params = curr;
	    first = 0;
	} else {
	    prev->next = curr;
	}
	prev = curr;
	param = param->next;
	
    }
    
    if (shm_str_dup(&c->c, _contact) < 0) goto error;
    if (shm_str_dup(&c->callid, _ci->callid) < 0) goto error;
    if (shm_str_dup(&c->user_agent, _ci->user_agent) < 0) goto error;
    if (shm_str_dup(&c->aor, _aor) < 0) goto error;
    if (shm_str_dup(&c->domain, _dom) < 0) goto error;
    
    if (_ci->received.s && _ci->received.len) {
        if (shm_str_dup(&c->received, &_ci->received) < 0) goto error;
    }
    if (_ci->path && _ci->path->len) {
        if (shm_str_dup(&c->path, _ci->path) < 0) goto error;
    }
    
    LM_DBG("generating hash based on [%.*s]\n", _contact->len, _contact->s);
    c->contact_hash = core_hash(_contact, 0, contact_list->size);
    c->ref_count = 1;
    c->expires = _ci->expires;
    c->q = _ci->q;
    c->sock = _ci->sock;
    c->cseq = _ci->cseq;
    c->flags = _ci->flags;
    c->cflags = _ci->cflags;
    c->methods = _ci->methods;
    c->last_modified = _ci->last_modified;

    return c;
error:
    LM_ERR("no more shm memory\n");
    if (c->path.s) shm_free(c->path.s);
    if (c->received.s) shm_free(c->received.s);
    if (c->user_agent.s) shm_free(c->user_agent.s);
    if (c->callid.s) shm_free(c->callid.s);
    if (c->c.s) shm_free(c->c.s);
    if (c->domain.s) shm_free(c->domain.s);
    if (c->aor.s) shm_free(c->aor.s);
    shm_free(c);
    return 0;
}

/*!
 * \brief Free all memory associated with given contact structure
 * \param _c freed contact
 */
void free_ucontact(ucontact_t* _c) {
    struct ul_callback *cbp, *cbp_tmp;
    struct contact_dialog_data *dialog_data, *tmp_dialog_data; 
    param_t * tmp, *tmp1;
    
    if (!_c) return;
    LM_DBG("Freeing ucontact [%.*s]\n", _c->aor.len, _c->aor.s);    
    if (_c->path.s) shm_free(_c->path.s);
    if (_c->received.s) shm_free(_c->received.s);
    if (_c->user_agent.s) shm_free(_c->user_agent.s);
    if (_c->callid.s) shm_free(_c->callid.s);
    if (_c->c.s) shm_free(_c->c.s);
    
    tmp = _c->params;
    while(tmp){
	tmp1 = tmp->next;
	if (tmp->body.s) shm_free(tmp->body.s);
	if (tmp->name.s) shm_free(tmp->name.s);
	if(tmp) shm_free(tmp);
	tmp = tmp1;
    }
    
    if (_c->domain.s) shm_free(_c->domain.s);
    if (_c->aor.s) shm_free(_c->aor.s);

    //free dialog data
    for (dialog_data = _c->first_dialog_data; dialog_data;) {
        tmp_dialog_data = dialog_data;
        dialog_data = dialog_data->next;
		shm_free(tmp_dialog_data);
    }
    
    //free callback list
    for (cbp = _c->cbs->first; cbp;) {
        cbp_tmp = cbp;
        cbp = cbp->next;
        if (cbp_tmp->param)
            shm_free(cbp_tmp->param);
        shm_free(cbp_tmp);
    }
    shm_free(_c->cbs);
    lock_dealloc(_c->lock);
    lock_destroy(_c->lock);
    shm_free(_c);
}

/*!
 * \brief Print contact, for debugging purposes only
 * \param _f output file
 * \param _c printed contact
 */
void print_ucontact(FILE* _f, ucontact_t* _c) {
    time_t t = time(0);
    char* st = "";
    param_t * tmp;
    
    fprintf(_f, "~~~Contact(%p)~~~\n", _c);
    fprintf(_f, "domain    : '%.*s'\n", _c->domain.len, ZSW(_c->domain.s));
    fprintf(_f, "aor       : '%.*s'\n", _c->aor.len, ZSW(_c->aor.s));
    fprintf(_f, "Contact   : '%.*s'\n", _c->c.len, ZSW(_c->c.s));
    
    fprintf(_f, "Params   :\n");
    tmp = _c->params;
    while (tmp) {
	fprintf(_f, "Param Name: '%.*s' Param Body '%.*s'\n", tmp->name.len, ZSW(tmp->name.s), tmp->body.len, ZSW(tmp->body.s));
	tmp = tmp->next;
    }
    
    
    fprintf(_f, "Expires   : ");
    if (_c->expires == 0) {
        fprintf(_f, "Permanent\n");
    } else if (_c->expires == UL_EXPIRED_TIME) {
        fprintf(_f, "Deleted\n");
    } else if (t > _c->expires) {
        fprintf(_f, "Expired\n");
    } else {
        fprintf(_f, "%u\n", (unsigned int) (_c->expires - t));
    }
    fprintf(_f, "q         : %s\n", q2str(_c->q, 0));
    fprintf(_f, "Call-ID   : '%.*s'\n", _c->callid.len, ZSW(_c->callid.s));
    fprintf(_f, "CSeq      : %d\n", _c->cseq);
    fprintf(_f, "User-Agent: '%.*s'\n",
            _c->user_agent.len, ZSW(_c->user_agent.s));
    fprintf(_f, "received  : '%.*s'\n",
            _c->received.len, ZSW(_c->received.s));
    fprintf(_f, "Path      : '%.*s'\n",
            _c->path.len, ZSW(_c->path.s));
    fprintf(_f, "State     : %s\n", st);
    fprintf(_f, "Flags     : %u\n", _c->flags);
    if (_c->sock) {
        fprintf(_f, "Sock      : %.*s (%p)\n",
                _c->sock->sock_str.len, _c->sock->sock_str.s, _c->sock);
    } else {
        fprintf(_f, "Sock      : none (null)\n");
    }
    fprintf(_f, "Methods   : %u\n", _c->methods);
    fprintf(_f, "next      : %p\n", _c->next);
    fprintf(_f, "prev      : %p\n", _c->prev);
    fprintf(_f, "~~~/Contact~~~~\n");
}

/*!
 * \brief Update existing contact in memory with new values
 * \param _c contact
 * \param _ci contact informations
 * \return 0 on success, -1 on failure
 */
int mem_update_ucontact(ucontact_t* _c, ucontact_info_t* _ci) {
#define update_str(_old,_new) \
	do{\
		if ((_old)->len < (_new)->len) { \
			ptr = (char*)shm_malloc((_new)->len); \
			if (ptr == 0) { \
				LM_ERR("no more shm memory\n"); \
				return -1; \
			}\
			memcpy(ptr, (_new)->s, (_new)->len);\
			if ((_old)->s) shm_free((_old)->s);\
			(_old)->s = ptr;\
		} else {\
			memcpy((_old)->s, (_new)->s, (_new)->len);\
		}\
		(_old)->len = (_new)->len;\
	} while(0)

    char* ptr;

    /* No need to update Callid as it is constant
     * per ucontact (set at insert time)  -bogdan */

    update_str(&_c->user_agent, _ci->user_agent);

    if (_ci->received.s && _ci->received.len) {
        update_str(&_c->received, &_ci->received);
    } else {
        if (_c->received.s) shm_free(_c->received.s);
        _c->received.s = 0;
        _c->received.len = 0;
    }

    if (_ci->path) {
        update_str(&_c->path, _ci->path);
    } else {
        if (_c->path.s) shm_free(_c->path.s);
        _c->path.s = 0;
        _c->path.len = 0;
    }
    
    LM_DBG("Setting contact expires to %d which is in %d seconds time\n", (unsigned int) _ci->expires, (unsigned int) (_ci->expires - time(NULL)));
    _c->sock = _ci->sock;
    _c->expires = _ci->expires;
    _c->q = _ci->q;
    _c->cseq = _ci->cseq;
    _c->methods = _ci->methods;
    _c->last_modified = _ci->last_modified;
    _c->flags = _ci->flags;
    _c->cflags = _ci->cflags;

    return 0;
}

/*!
 * \brief Setting contact expires to now in memory
 * \param _c contact
  * \return 0 on success, -1 on failure
 */
int mem_expire_ucontact(ucontact_t* _c) {
    get_act_time();
    _c->expires = act_time;

    return 0;
}



/*!
 * \brief Insert a new contact into the list at the correct position
 * \param _r record that holds the sorted contacts
 * \param _c new contact
 */
static inline void update_contact_pos(struct impurecord* _r, ucontact_t* _c) {
    ucontact_t *pos, *ppos;

    if (_c->next == 0) //if its last its newest already
        return;

    if (_c->prev == 0) //must be only element in the list
        return;

    if (_c->next->expires < _c->expires) {//changed place required
        ppos = _c->next;
        pos = _c->next->next;
        //unlink _c
        _c->prev->next = _c->next;
        _c->next->prev = _c->prev;
        _c->prev = _c->next = 0;

        while (pos) {
            if (_c->expires < pos->expires)
                break;
            ppos = pos;
            pos = pos->next;
        }

        ppos->next = _c;
        _c->prev = ppos;
        if (pos) {
            _c->next = pos;
            pos->prev = _c;
        }
    }
}

/*!
 * \brief Setting ucontact expires to now
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \return 0 on success, -1 on failure
 */
int expire_ucontact(struct impurecord* _r, ucontact_t* _c) {
    /* we have to update memory in any case, but database directly
     * only in db_mode 1 */
    LM_DBG("Expiring contact aor: [%.*s] and contact uri: [%.*s]\n", _c->aor.len, _c->aor.s, _c->c.len, _c->c.s);
    if (mem_expire_ucontact(_c) < 0) {
        LM_ERR("failed to update memory\n");
        return -1;
    }
    
    if (db_mode == WRITE_THROUGH && (db_insert_ucontact(_r, _c) != 0)) {  /* this is an insert/update */
	LM_ERR("failed to update contact in DB [%.*s]\n", _c->aor.len, _c->aor.s);
	return -1;
    }
    
    //make sure IMPU is linked to this contact
    link_contact_to_impu(_r, _c, 1);

    /* run callbacks for UPDATE event */
    if (exists_ulcb_type(_c->cbs, UL_CONTACT_EXPIRE)) {
        LM_DBG("exists callback for type= UL_CONTACT_UPDATE\n");
        run_ul_callbacks(_c->cbs, UL_CONTACT_EXPIRE, _r, _c);
    }
    if (exists_ulcb_type(_r->cbs, UL_IMPU_EXPIRE_CONTACT)) {
        run_ul_callbacks(_r->cbs, UL_IMPU_EXPIRE_CONTACT, _r, _c);
    }

    return 0;
}



/*!
 * \brief Update ucontact with new values
 * \param _r record the contact belongs to
 * \param _c updated contact
 * \param _ci new contact informations
 * \return 0 on success, -1 on failure
 */
int update_ucontact(struct impurecord* _r, ucontact_t* _c, ucontact_info_t* _ci) {
    /* we have to update memory in any case, but database directly
     * only in db_mode 1 */
    LM_DBG("Updating contact aor: [%.*s] and contact uri: [%.*s]\n", _c->aor.len, _c->aor.s, _c->c.len, _c->c.s);
    if (mem_update_ucontact(_c, _ci) < 0) {
        LM_ERR("failed to update memory\n");
        return -1;
    }
    
    if (db_mode == WRITE_THROUGH && (db_insert_ucontact(_r, _c) != 0)) {  /* this is an insert/update */
	LM_ERR("failed to update contact in DB [%.*s]\n", _c->aor.len, _c->aor.s);
	return -1;
    }
    
    //make sure IMPU is linked to this contact
    link_contact_to_impu(_r, _c, 1);

    /* run callbacks for UPDATE event */
    if (exists_ulcb_type(_c->cbs, UL_CONTACT_UPDATE)) {
        LM_DBG("exists callback for type= UL_CONTACT_UPDATE\n");
        run_ul_callbacks(_c->cbs, UL_CONTACT_UPDATE, _r, _c);
    }
    if (exists_ulcb_type(_r->cbs, UL_IMPU_UPDATE_CONTACT)) {
        run_ul_callbacks(_r->cbs, UL_IMPU_UPDATE_CONTACT, _r, _c);
    }

//    update_contact_pos(_r, _c);

    return 0;
}


/*!
 * \brief Add dialog data to contact
 * used when this contact is part of a confirmed dialog so we can tear down the dialog if the contact is removed
 */
int add_dialog_data_to_contact(ucontact_t* _c, unsigned int h_entry, unsigned int h_id) {
    
    struct contact_dialog_data *dialog_data = (struct contact_dialog_data*)shm_malloc(sizeof( struct contact_dialog_data));
    
    LM_DBG("Adding dialog data to contact <%.*s> with h_entry <%d> and h_id <%d>", _c->c.len, _c->c.s, h_entry, h_id);

    memset(dialog_data, 0, sizeof( struct contact_dialog_data));
    
    dialog_data->h_entry = h_entry;
    dialog_data->h_id = h_id;
    dialog_data->next = 0;
    dialog_data->prev = 0;
    
    if(_c->first_dialog_data==0){
	//first entry in the list
	_c->first_dialog_data = dialog_data;
	_c->last_dialog_data = dialog_data;
    }else {
	//not first entry in list
	_c->last_dialog_data->next = dialog_data;
	dialog_data->prev = _c->last_dialog_data;
	_c->last_dialog_data = dialog_data;
    }
    
    return 0;
    
}

/*!
 * \brief Add dialog data to contact
 * used when this contact is part of a confirmed dialog so we can tear down the dialog if the contact is removed
 */
int remove_dialog_data_from_contact(ucontact_t* _c, unsigned int h_entry, unsigned int h_id) {
    struct contact_dialog_data *dialog_data, *tmp_dialog_data; 
    LM_DBG("Removing dialog data from contact <%.*s> with h_entry <%d> and h_id <%d>", _c->c.len, _c->c.s, h_entry, h_id);
    
    for (dialog_data = _c->first_dialog_data; dialog_data;) {
        tmp_dialog_data = dialog_data;
        dialog_data = dialog_data->next;
	if(tmp_dialog_data->h_entry == h_entry && tmp_dialog_data->h_id == h_id){
	    LM_DBG("Found matching dialog data so will remove it");
	    if(tmp_dialog_data->prev) {
		tmp_dialog_data->prev->next = tmp_dialog_data->next;
	    } else {
		_c->first_dialog_data =  tmp_dialog_data->next;
	    }
	    if(tmp_dialog_data->next){
		tmp_dialog_data->next->prev = tmp_dialog_data->prev;
	    }else{
	       _c->last_dialog_data =  tmp_dialog_data->prev;
	    }
	    shm_free(tmp_dialog_data);
	    return 0;
	}
        
    }
    LM_DBG("Did not find dialog data to remove from contact");
    return 0;
}

void release_ucontact(struct ucontact* _c) {
    lock_contact_slot_i(_c->contact_hash);
    _c->ref_count--;
    unlock_contact_slot_i(_c->contact_hash);
}
