/* 
 * File:   ro_session_hash.c
 * Author: Jason Penton
 *
 * Created on 08 April 2011, 1:10 PM
 */

#include "ro_session_hash.h"

#define MAX_ROSESSION_LOCKS  2048
#define MIN_ROSESSION_LOCKS  2

/*! global ro_session table */
struct ro_session_table *ro_session_table = 0;

/*!
 * \brief Link a ro_session structure
 * \param ro_session Ro Session
 * \param n extra increments for the reference counter
 */
void link_ro_session(struct ro_session *ro_session, int n) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);

    ro_session->h_id = ro_session_entry->next_id++;
    if (ro_session_entry->first == 0) {
        ro_session_entry->first = ro_session_entry->last = ro_session;
    } else {
        ro_session_entry->last->next = ro_session;
        ro_session->prev = ro_session_entry->last;
        ro_session_entry->last = ro_session;
    }

    ro_session->ref += 1 + n;
    
    ro_session_unlock(ro_session_table, ro_session_entry);

    return;
}

/*!
 * \brief Refefence an ro_session with locking
 * \see ref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt increment for the reference counter
 */
void ref_ro_session(struct ro_session *ro_session, unsigned int cnt) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);
    ref_ro_session_unsafe(ro_session, cnt);
    ro_session_unlock(ro_session_table, ro_session_entry);
}

/*!
 * \brief Unreference an ro_session with locking
 * \see unref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt decrement for the reference counter
 */
void unref_ro_session(struct ro_session *ro_session, unsigned int cnt) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);
    unref_ro_session_unsafe(ro_session, cnt, ro_session_entry);
    ro_session_unlock(ro_session_table, ro_session_entry);
}

/*!
 * \brief Initialize the global ro_session table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ro_session_table(unsigned int size) {
    unsigned int n;
    unsigned int i;

    ro_session_table = (struct ro_session_table*) shm_malloc(sizeof (struct ro_session_table) + size * sizeof (struct ro_session_entry));
    if (ro_session_table == 0) {
        LM_ERR("no more shm mem (1)\n");
        goto error0;
    }

    memset(ro_session_table, 0, sizeof (struct ro_session_table));
    ro_session_table->size = size;
    ro_session_table->entries = (struct ro_session_entry*) (ro_session_table + 1);

    n = (size < MAX_ROSESSION_LOCKS) ? size : MAX_ROSESSION_LOCKS;
    for (; n >= MIN_ROSESSION_LOCKS; n--) {
        ro_session_table->locks = lock_set_alloc(n);
        if (ro_session_table->locks == 0)
            continue;
        if (lock_set_init(ro_session_table->locks) == 0) {
            lock_set_dealloc(ro_session_table->locks);
            ro_session_table->locks = 0;
            continue;
        }
        ro_session_table->locks_no = n;
        break;
    }

    if (ro_session_table->locks == 0) {
        LM_ERR("unable to allocate at least %d locks for the hash table\n",
                MIN_ROSESSION_LOCKS);
        goto error1;
    }

    for (i = 0; i < size; i++) {
        memset(&(ro_session_table->entries[i]), 0, sizeof (struct ro_session_entry));
        ro_session_table->entries[i].next_id = rand() % (3*size);
        ro_session_table->entries[i].lock_idx = i % ro_session_table->locks_no;
    }

    return 0;
error1:
    shm_free(ro_session_table);
    ro_session_table = NULL;
error0:
    return -1;
}

/*!
 * \brief Destroy an ro_session and free memory
 * \param ro_session destroyed Ro Session
 */
inline void destroy_ro_session(struct ro_session *ro_session) {

    LM_DBG("destroying Ro Session %p\n", ro_session);

    remove_ro_timer(&ro_session->ro_tl);
    
    if (ro_session->ro_session_id.s && (ro_session->ro_session_id.len > 0)) {
        shm_free(ro_session->ro_session_id.s);
    }


    shm_free(ro_session);
}

/*!
 * \brief Destroy the global Ro session table
 */
void destroy_dlg_table(void) {
    struct ro_session *ro_session, *l_ro_session;
    unsigned int i;

    if (ro_session_table == 0)
        return;

    if (ro_session_table->locks) {
        lock_set_destroy(ro_session_table->locks);
        lock_set_dealloc(ro_session_table->locks);
    }

    for (i = 0; i < ro_session_table->size; i++) {
        ro_session = ro_session_table->entries[i].first;
        while (ro_session) {
            l_ro_session = ro_session;
            ro_session = ro_session->next;
            destroy_ro_session(l_ro_session);
        }

    }

    shm_free(ro_session_table);
    ro_session_table = 0;

    return;
}

struct ro_session* build_new_ro_session(int direction, int auth_appid, int auth_session_type, str *session_id, str *callid, str *asserted_identity, 
	str* called_asserted_identity, str* mac, unsigned int dlg_h_entry, unsigned int dlg_h_id, unsigned int requested_secs, unsigned int validity_timeout,
	int active_rating_group, int active_service_identifier, str *incoming_trunk_id, str *outgoing_trunk_id, str *pani){
    LM_DBG("Building Ro Session **********");
    char *p;
    unsigned int len = session_id->len + callid->len + asserted_identity->len + called_asserted_identity->len + mac->len + 
        incoming_trunk_id->len + outgoing_trunk_id->len + pani->len + sizeof (struct ro_session);
    struct ro_session *new_ro_session = (struct ro_session*) shm_malloc(len);

    if (!new_ro_session) {
        LM_ERR("no more shm mem.\n");
        shm_free(new_ro_session);
        return 0;
    }
    
    LM_DBG("New Ro Session given memory at address [%p]\n", new_ro_session);

    memset(new_ro_session, 0, len);
    
//    if (pani->len < MAX_PANI_LEN) {
//		p = new_ro_session->pani;
//		memcpy(p, pani->s, pani->len);
//    }

    new_ro_session->direction = direction;
    new_ro_session->auth_appid = auth_appid;
    new_ro_session->auth_session_type = auth_session_type;

    new_ro_session->ro_tl.next = new_ro_session->ro_tl.prev;
    new_ro_session->ro_tl.timeout = 0; //requested_secs;

    new_ro_session->reserved_secs = requested_secs;
    new_ro_session->valid_for = validity_timeout;

    new_ro_session->hop_by_hop = 1;
    new_ro_session->next = 0;
    new_ro_session->dlg_h_entry = dlg_h_entry;
    new_ro_session->dlg_h_id = dlg_h_id;

    new_ro_session->h_entry = dlg_h_entry; /* we will use the same entry ID as the dlg - saves us using our own hash function */
    new_ro_session->h_id = 0;
    new_ro_session->ref = 0;
    
    new_ro_session->rating_group = active_rating_group;
    new_ro_session->service_identifier = active_service_identifier;

    p = (char*) (new_ro_session + 1);
    new_ro_session->callid.s = p;
    new_ro_session->callid.len = callid->len;
    memcpy(p, callid->s, callid->len);
    p += callid->len;

    new_ro_session->ro_session_id.s = p;
    new_ro_session->ro_session_id.len = session_id->len;
    memcpy(p, session_id->s, session_id->len);
    p += session_id->len;

    new_ro_session->asserted_identity.s = p;
    new_ro_session->asserted_identity.len = asserted_identity->len;
    memcpy(p, asserted_identity->s, asserted_identity->len);
    p += asserted_identity->len;

    new_ro_session->called_asserted_identity.s = p;
    new_ro_session->called_asserted_identity.len = called_asserted_identity->len;
    memcpy(p, called_asserted_identity->s, called_asserted_identity->len);
    p += called_asserted_identity->len;
    
    new_ro_session->incoming_trunk_id.s = p;
    new_ro_session->incoming_trunk_id.len = incoming_trunk_id->len;
    memcpy(p, incoming_trunk_id->s, incoming_trunk_id->len);
    p += incoming_trunk_id->len;
    
    new_ro_session->outgoing_trunk_id.s = p;
    new_ro_session->outgoing_trunk_id.len = outgoing_trunk_id->len;
    memcpy(p, outgoing_trunk_id->s, outgoing_trunk_id->len);
    p += outgoing_trunk_id->len;
    
    new_ro_session->avp_value.mac.s = p;
    new_ro_session->avp_value.mac.len = mac->len;
    memcpy(p, mac->s, mac->len);
    p += mac->len;
    
    new_ro_session->pani.s = p;
    memcpy(p, pani->s, pani->len);
    new_ro_session->pani.len = pani->len;
    p += pani->len;

    if (p != (((char*) new_ro_session) + len)) {
        LM_ERR("buffer overflow\n");
        shm_free(new_ro_session);
        return 0;
    }

    return new_ro_session;

}

/*!
 * \brief Lookup an Ro session in the global list
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return ro_session on success, NULL on failure
 */
struct ro_session* lookup_ro_session(unsigned int h_entry, str* callid, int direction, unsigned int *del) {
    struct ro_session *ro_session;
    struct ro_session_entry *ro_session_entry;

    if (del != NULL)
        *del = 0;

    if (h_entry >= ro_session_table->size)
        goto not_found;
    ro_session_entry = &(ro_session_table->entries[h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);

    for (ro_session = ro_session_entry->first; ro_session; ro_session = ro_session->next) {
        if ((direction==0 || direction==ro_session->direction) && (strncmp(ro_session->callid.s, callid->s, callid->len)==0)) {
		ref_ro_session_unsafe(ro_session,1);
            LM_DBG("ref ro_session %p with 1 -> %d\n", ro_session, ro_session->ref);
            ro_session_unlock(ro_session_table, ro_session_entry);
            LM_DBG("ro_session id=%u found on entry %u\n", ro_session->h_id, h_entry);
            return ro_session;
        }
    }

    ro_session_unlock(ro_session_table, ro_session_entry);
not_found:
    LM_DBG("no ro_session for callid=%.*s found on entry %u\n", callid->len, callid->s, h_entry);
    return 0;
}

/*
 * \brief free impu_data parcel
 */
void free_impu_data(struct impu_data *impu_data) {
    if(impu_data){
	shm_free(impu_data);
	impu_data=0;
    }
}
