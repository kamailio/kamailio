/* 
 * File:   ro_session_hash.h
 * Author: Jason Penton
 *
 * Created on 07 April 2011, 4:12 PM
 */

#ifndef RO_SESSION_HASH_H
#define	RO_SESSION_HASH_H

#include "ro_timer.h"
#include "../../mem/shm_mem.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include <stdlib.h>


/* ro session flags */
#define RO_SESSION_FLAG_NEW          (1<<0) /*!< new ro session */
#define RO_SESSION_FLAG_INSERTED     (1<<1) /*!< session has been written to DB */
#define RO_SESSION_FLAG_CHANGED      (1<<2) /*!< ro session has been updated */
#define RO_SESSION_FLAG_DELETED      (1<<3) /*!< ro session has been deleted */

#define MAX_PANI_LEN 100

enum ro_session_event_type {
    pending,
    answered,
    no_more_credit,
    unknown_error
};

struct diameter_avp_value {
	str mac;
};

//used to pass data into dialog callbacks
struct impu_data {
    str identity;
    str contact;
} impu_data_t;


struct ro_session {
    str cdp_session_id;
    volatile int ref;
    int direction;
    struct ro_session* next;
    struct ro_session* prev;
    str ro_session_id;
    str callid;
    str asserted_identity;
    str called_asserted_identity;
    str incoming_trunk_id;
    str outgoing_trunk_id;
    str pani;
    unsigned int hop_by_hop;
    struct ro_tl ro_tl;
    unsigned int reserved_secs;
    unsigned int valid_for;
    unsigned int dlg_h_entry;
    unsigned int dlg_h_id;
    unsigned int h_entry;
    unsigned int h_id;
    time_t start_time;
    time_t last_event_timestamp;
    enum ro_session_event_type event_type;
    int auth_appid;
    int auth_session_type;
    int active;
    unsigned int flags;
    struct diameter_avp_value avp_value;
    int rating_group;
    int service_identifier;
};

/*! entries in the main ro_session table */
struct ro_session_entry {
    struct ro_session *first; /*!< dialog list */
    struct ro_session *last; /*!< optimisation, end of the dialog list */
    unsigned int next_id; /*!< next id */
    unsigned int lock_idx; /*!< lock index */
};

/*! main ro_sesion table */
struct ro_session_table {
    unsigned int size; /*!< size of the dialog table */
    struct ro_session_entry *entries; /*!< dialog hash table */
    unsigned int locks_no; /*!< number of locks */
    gen_lock_set_t *locks; /*!< lock table */
};


/*! global ro_session table */
extern struct ro_session_table *ro_session_table;


/*!
 * \brief Set a ro_session lock
 * \param _table ro_session table
 * \param _entry locked entry
 */
#define ro_session_lock(_table, _entry) \
		{ LM_DBG("LOCKING %d", (_entry)->lock_idx); lock_set_get( (_table)->locks, (_entry)->lock_idx); LM_DBG("LOCKED %d", (_entry)->lock_idx);}


/*!
 * \brief Release a ro_session lock
 * \param _table ro_session table
 * \param _entry locked entry
 */
#define ro_session_unlock(_table, _entry) \
		{ LM_DBG("UNLOCKING %d", (_entry)->lock_idx); lock_set_release( (_table)->locks, (_entry)->lock_idx); LM_DBG("UNLOCKED %d", (_entry)->lock_idx); }

/*!
 * \brief Reference an ro_session without locking
 * \param _ro_session Ro Session
 * \param _cnt increment for the reference counter
 */
#define ref_ro_session_unsafe(_session,_cnt)     \
	do { \
		(_session)->ref += (_cnt); \
		LM_DBG("ref ro_session %p with %d -> %d (tl=%p)\n", \
			(_session),(_cnt),(_session)->ref,&(_session)->ro_tl); \
	}while(0)


/*!
 * \brief Unreference an ro_session without locking
 * \param _ro_session Ro Session
 * \param _cnt decrement for the reference counter
 */
#define unref_ro_session_unsafe(_ro_session,_cnt,_ro_session_entry)   \
	do { \
		(_ro_session)->ref -= (_cnt); \
		LM_DBG("unref ro_session %p with %d -> %d (tl=%p)\n",\
			(_ro_session),(_cnt),(_ro_session)->ref,&(_ro_session)->ro_tl);\
		if ((_ro_session)->ref<0) {\
			LM_CRIT("bogus ref for session id < 0 [%d]\n",(_ro_session)->ref);\
		}\
		if ((_ro_session)->ref<=0) { \
			unlink_unsafe_ro_session( _ro_session_entry, _ro_session);\
			LM_DBG("ref <=0 for ro_session %p\n",_ro_session);\
			destroy_ro_session(_ro_session);\
		}\
	}while(0)

/*!
 * \brief Unlink a ro_session from the list without locking
 * \see unref_ro_session_unsafe
 * \param ro_session_entry unlinked entry
 * \param ro_session unlinked ro_session
 */
static inline void unlink_unsafe_ro_session(struct ro_session_entry *ro_session_entry, struct ro_session *ro_session) {
    if (ro_session->next)
        ro_session->next->prev = ro_session->prev;
    else
        ro_session_entry->last = ro_session->prev;
    if (ro_session->prev)
        ro_session->prev->next = ro_session->next;
    else
        ro_session_entry->first = ro_session->next;

    ro_session->next = ro_session->prev = 0;

    return;
}

/*!
 * \brief Initialize the global ro_session table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ro_session_table(unsigned int size);

/*!
 * \brief Destroy a ro_session and free memory
 * \param ro_session destroyed Ro Session
 */
inline void destroy_ro_session(struct ro_session *ro_session);

/*!
 * \brief Destroy the ro_session dialog table
 */
void destroy_ro_session_table(void);

/*!
 * \brief Link a ro_session structure
 * \param ro_session Ro Session
 * \param n extra increments for the reference counter
 */
void link_ro_session(struct ro_session *ro_session, int n);

void remove_aaa_session(str *session_id);

struct ro_session* build_new_ro_session(int direction, int auth_appid, int auth_session_type, str *session_id, str *callid, str *asserted_identity, str* called_asserted_identity, 
	str* mac, unsigned int dlg_h_entry, unsigned int dlg_h_id, unsigned int requested_secs, unsigned int validity_timeout,
	int active_rating_group, int active_service_identifier, str *incoming_trunk_id, str *outgoing_trunk_id, str *pani);

/*!
 * \brief Refefence a ro_session with locking
 * \see ref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt increment for the reference counter
 */
void ref_ro_session(struct ro_session *ro_session, unsigned int cnt);

/*!
 * \brief Unreference a ro_session with locking
 * \see unref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt decrement for the reference counter
 */
void unref_ro_session(struct ro_session *ro_session, unsigned int cnt);

struct ro_session* lookup_ro_session(unsigned int h_entry, str *callid, int direction, unsigned int *del);

void free_impu_data(struct impu_data *impu_data);


#endif	/* RO_SESSION_HASH_H */

