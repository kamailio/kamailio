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

#ifndef USRLOC_H
#define USRLOC_H

#include <time.h>
#include "ul_callback.h"
#include "../../qvalue.h"
#include "../../str.h"
#include "../../modules/tm/dlg.h"
#include "../cdp/diameter_ims_code_avp.h"

#define DEFAULT_DBG_FILE "/var/log/usrloc_debug"
#define MAX_CONTACTS_PER_IMPU 100

/* DB modes */
#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2		//not implemented yet
#define DB_ONLY	      3		//not implemented yet

/*IMPU states*/
#define IMS_USER_NOT_REGISTERED 0		/** User not registered */
#define IMS_USER_REGISTERED 1			/** User registered */
#define IMS_USER_UNREGISTERED -1		/** User unregistered (not registered but with services for unregistered state) */

/** Conjunctive Normal Format */
#define IFC_CNF 1
/** Disjunctive Normal Format */
#define IFC_DNF 0

#define IFC_UNKNOWN -1				/** unknown SPT type */
#define IFC_REQUEST_URI 1			/** SPT for checking the Request-URI */
#define IFC_METHOD 2				/** SPT for checking the Method */
#define IFC_SIP_HEADER 3			/** SPT for checking a SIP Header */
#define IFC_SESSION_CASE 4			/** SPT for checking the Session Case */
#define IFC_SESSION_DESC 5			/** SPT for checking a SDP line */

#define IFC_ORIGINATING_SESSION 0	/** Session case originating */
#define IFC_TERMINATING_SESSION 1	/** Session case terminating */
#define IFC_TERMINATING_UNREGISTERED 2	/** Session case terminating to unregistered user*/

#define IFC_INITIAL_REGISTRATION 	1		/** Initial Registration */
#define IFC_RE_REGISTRATION 		1<<1	/** Re-Registration */
#define IFC_DE_REGISTRATION 		1<<2	/** De-Registration */

#define IFC_NO_DEFAULT_HANDLING -1	/** No default handling */
#define IFC_SESSION_CONTINUED 0		/** Session should continue on failure to contact AS */
#define IFC_SESSION_TERMINATED 1	/** Session should be terminated on failure to contact AS */


/*forward declaration necessary for udomain*/
struct udomain;
typedef struct udomain udomain_t;


typedef struct _subscriber_data {
	int event;
	int expires;
	int version;
	str* callid;
	str* ftag;
	str* ttag;
	unsigned int local_cseq;
	str* record_route;
	str* sockinfo_str;
	str* presentity_uri;
	str *watcher_uri;
	str* watcher_contact;
} subscriber_data_t;

typedef struct _reg_subscriber {
    int event;
    time_t expires; /**< Time of expiration		 			*/
    int version; /**< Last version sent to this subs.	*/

    str watcher_uri;
    str watcher_contact;
    str presentity_uri;
    
    unsigned int local_cseq;
    str call_id;
    str from_tag;
    str to_tag;
    str record_route;
    str sockinfo_str;

    struct _reg_subscriber *next; /**< the next subscriber in the list		*/
    struct _reg_subscriber *prev; /**< the previous subscriber in the list	*/
} reg_subscriber;

/*!
 * \brief States for in-memory contacts in regards to contact storage handler (db, in-memory, ldap etc)
 */
typedef enum cstate {
    CS_NEW, /*!< New contact - not flushed yet */
    CS_SYNC, /*!< Synchronized contact with the database */
    CS_DIRTY /*!< Update contact - not flushed yet */
} cstate_t;

typedef enum contact_state {
    CONTACT_VALID,
    CONTACT_EXPIRED,
    CONTACT_DELETED
} contact_state_t;

/*! \brief Valid contact is a contact that either didn't expire yet or is permanent */
#define VALID_CONTACT(c, t)   ((c->expires>t) || (c->expires==0))

struct hslot;
/*!< Hash table slot */
struct socket_info;

/** SPT for checking a SIP Header */
typedef struct _ims_sip_header {
    str header; /**< name of the header to match	*/
    str content; /**< regex to match             	*/
    short type; /**< if known header, precalculated	*/
} ims_sip_header;

/** SPT for checking a SDP line */
typedef struct _ims_session_desc {
    str line; /**< name of line from description */
    str content; /**< regex to match                */
} ims_session_desc;

/** Service Point Trigger Structure */
typedef struct _ims_spt {
    char condition_negated; /**< if to negate entire condition	*/
    int group; /**< group to which it belongs		*/
    char type; /**< type of condition				*/

    union {
        str request_uri; /**< Request URI regex				*/
        str method; /**< the SIP method should be this	*/
        ims_sip_header sip_header; /**< match of a certain SIP header	*/
        char session_case; /**< session direction and case		*/
        ims_session_desc session_desc; /**< session description match 		*/
    };
    /**< union for SPT 					*/
    char registration_type; /**< set of registration types		*/
} ims_spt;

/** Trigger Point Structure */

typedef struct _ims_trigger_point {
    char condition_type_cnf; /**< if it's CNF or DNF     		*/
    ims_spt *spt; /**< service point triggers 1..n 		*/
    unsigned short spt_cnt; /**< number of service point triggers 	*/
} ims_trigger_point;

/** Application Server Structure */
typedef struct _ims_application_server {
    str server_name; /**< SIP URL of the app server                      */
    char default_handling; /**< enum SESSION_CONTINUED SESSION_TERMINATED 0..1 */
    str service_info; /**< optional info to be sent to AS 0..1            */
} ims_application_server;

/** Public Identity Structure */
typedef struct {
    char barring; /**< Barring state									*/
    str public_identity; /**< Public Identity string							*/
    str wildcarded_psi; /** if exists is the wildcarded psi					*/
} ims_public_identity;

/** Initial Filter Criteria Structure */
typedef struct _ims_filter_criteria {
    int priority; /**< checking priority, lower means more important */
    ims_trigger_point *trigger_point; /**< definition of trigger 0..1 */
    ims_application_server application_server; /**< target of the trigger   */
    char *profile_part_indicator; /**< profile part indicator 0..1 */
} ims_filter_criteria;

/** CoreNetwork Service Authorization */
typedef struct _ims_cn_service_auth {
    int subscribed_media_profile_id; /* must be >=0 */
} ims_cn_service_auth;

/** Service Profile Structure */
typedef struct {
    ims_public_identity *public_identities; /**< array of public identities		*/
    unsigned short public_identities_cnt; /**< number of public identities	*/
    ims_filter_criteria *filter_criteria; /**< vector of filter criteria 0..n */
    unsigned short filter_criteria_cnt; /**< size of the vector above		*/
    ims_cn_service_auth *cn_service_auth; /**< core net. services auth. 0..1	*/
    int *shared_ifc_set; /**< shared ifc set ids 0..n 		*/
    unsigned short shared_ifc_set_cnt; /**< size of above vector 			*/
} ims_service_profile;

/** User Subscription Structure */
typedef struct ims_subscription_s {
    str private_identity; /**< private identity 				*/
    struct hslot_sp* slot; /*!< Collision slot in the hash table array we belong to */
    unsigned int impu_hash; /**< hash over public_identity */
    int wpsi; /** This is not in the standards
	 	 	 	 	 	 	 	 	 	 	 	0 normal user or distinct psi inside
	 	 	 	 	 	 	 	 	 	 	 	1 wildcarded psi
	 	 	 	 	 	 	 	 	 	 	 **/
    ims_service_profile *service_profiles; /**< array of service profiles		*/
    unsigned short service_profiles_cnt; /**< size of the array above		*/

    int ref_count; /**< referenced count 				*/
    gen_lock_t *lock; /**< lock for operations on it 		*/
    struct ims_subscription_s* next;
    struct ims_subscription_s* prev;
} ims_subscription;

/** IPSec SA Information */
typedef struct ipsec {
    unsigned int spi_uc; /**< SPI Client to use					*/
    unsigned int spi_us; /**< SPI Server to use					*/
    unsigned int spi_pc; /**< SPI Client to use					*/
    unsigned int spi_ps; /**< SPI Server to use					*/
    unsigned short port_uc; /**< Port UE Client						*/
    unsigned short port_us; /**< Port UE Server						*/

    str ealg; /**< Cypher Algorithm - ESP				*/
    str r_ealg; /**< received Cypher Algorithm - ESP	*/
    str ck; /**< Cypher Key							*/
    str alg; /**< Integrity Algorithm - AH			*/
    str r_alg; /**<received Integrity Algorithm - AH	*/
    str ik; /**< Integrity Key						*/
    str prot; /**< Protocol (ah/esp) */
    str mod; /**< Mode (transport/tunnel) */
} ipsec_t;

typedef enum sec_type {
    SECURITY_NONE = 0,
    SECURITY_IPSEC = 1,
} security_type;

typedef struct security {
    str sec_header; /**< Security Header value 				*/
    security_type type; /**< Type of security in use			*/

    union {
        ipsec_t *ipsec; /**< IPSec SA information, if any		*/
    } data;
    float q;
} security_t;

/*! \brief Structure to hold dialog data used when this contact is part of a confirmed dialog so we can tear down the dialog if the contact is removed */
typedef struct contact_dialog_data {
    unsigned int h_entry;
    unsigned int h_id;

    struct contact_dialog_data* next; /*!< Next contact in the linked list */
    struct contact_dialog_data* prev; /*!< Previous contact in the linked list */
} contact_dialog_data_t;

/*! \brief Main structure for handling of registered Contact data */
typedef struct ucontact {
    gen_lock_t *lock;           /**< we have to lock the contact as it is shared by many impu structs and has reference conting	*/
    struct contact_hslot* slot; /*!< Collision slot in the hash table array we belong to */
    unsigned int contact_hash; 			/*!< Hash over contact */
    int ref_count;
    contact_state_t state;
    str domain; /*!< Pointer to domain name (NULL terminated) */
    str aor; /*!< Pointer to the AOR string in record structure*/
    str c; /*!< Contact address */
    param_t *params; /*!< Params header details> */
    str received; /*!< IP+port+protocol we received the REGISTER from */
    str path; /*!< Path header */
    time_t expires; /*!< Expires parameter */
    qvalue_t q; /*!< q parameter */
    str callid; /*!< Call-ID header field of registration */
    int cseq; /*!< CSeq value */
//    cstate_t state; /*!< State of the contact (\ref cstate) */
    unsigned int flags; /*!< Various flags (NAT, ping type, etc) */
    unsigned int cflags; /*!< Custom contact flags (from script) */
    str user_agent; /*!< User-Agent header field */
    struct socket_info *sock; /*!< received socket */
    time_t last_modified; /*!< When the record was last modified */
    unsigned int methods; /*!< Supported methods */

    struct socket_info *si_ps;
    struct socket_info *sipc;
    security_t *security_temp; /**< Security-Client Information		*/
    security_t *security; /**< Security-Client Information		*/

    struct ulcb_head_list* cbs;	/**< individual callbacks per contact */
    
    struct contact_dialog_data *first_dialog_data;
    struct contact_dialog_data *last_dialog_data;

    struct ucontact* next; /*!< Next contact in the linked list */
    struct ucontact* prev; /*!< Previous contact in the linked list */
} ucontact_t;

/*! \brief Informations related to a contact (used mainly for passing data around */
typedef struct ucontact_info {
    str received; /*!< Received interface */
    str* path; /*!< Path informations */
    time_t expires; /*!< Contact expires */
    qvalue_t q; /*!< Q-value */
    str* callid; /*!< call-ID */
    param_t *params;
    int cseq; /*!< CSEQ number */
    unsigned int flags; /*!< message flags */
    unsigned int cflags; /*!< contact flags */
    str *user_agent; /*!< user agent header */
    struct socket_info *sock; /*!< socket informations */

    unsigned int methods; /*!< supported methods */
    time_t last_modified; /*!< last modified */
} ucontact_info_t;

typedef struct udomain_head {
    str* name;
} udomain_head_t;

/** Enumeration for public identity Registration States */
enum pi_reg_states {
    IMPU_NOT_REGISTERED = 0, 	/**< User not-registered, no profile stored	*/
    IMPU_REGISTERED = 1, 		/**< User registered						*/
    IMPU_UNREGISTERED = -1 		/**< User not-registered, profile stored	*/
};

static inline char* get_impu_regstate_as_string(enum pi_reg_states reg_state) {
    switch (reg_state) {
        case IMPU_NOT_REGISTERED:
            return "not registered";
        case IMPU_REGISTERED:
            return "registered";
        case IMPU_UNREGISTERED:
            return "unregistered";
        default:
            return "unknown";
    }
}

/*! \brief
 * Basic hash table element
 */
typedef struct impurecord {
    str* domain; 					/*!< Pointer to domain we belong to (null terminated string) */
    int is_primary;					/*!< first IMPU (in implicit set this is the one that will trigger a SAR, if no implicit set - we should still be safe with first) */
    str public_identity; 			/*!< Address of record */
    unsigned int aorhash; 			/*!< Hash over address of record */
    int barring;
    enum pi_reg_states reg_state;
    ims_subscription *s; 			/**< subscription to which it belongs 		*/
    str ccf1, ccf2, ecf1, ecf2; 	/**< charging functions						*/
    ucontact_t* newcontacts[MAX_CONTACTS_PER_IMPU];
    int num_contacts;
    reg_subscriber *shead, *stail; 	/**< list of subscribers attached			*/
    time_t expires; 				/*!< timer when this IMPU expires - currently only used for unreg IMPU */
    int send_sar_on_delete;			/* used to distinguish between explicit contact removal and contact expiry - SAR only sent on contact expiry*/

    struct hslot* slot; 			/*!< Collision slot in the hash table array we belong to */
    struct ulcb_head_list* cbs;		/**< individual callbacks per impurecord */
    struct impurecord* prev; 		/*!< Next item in the hash entry */
    struct impurecord* next; 		/*!< Previous item in the hash entry */
} impurecord_t;

/** a parcel for transporting impurecord information */
typedef struct impurecord_info {
    int barring;
    enum pi_reg_states reg_state;
    ims_subscription *s;
    str *ccf1, *ccf2, *ecf1, *ecf2;
} impurecord_info_t;

typedef struct contact_list {
    struct contact_hslot* slot;
    int size;
//    stat_var *contacts;        /*!< no of contacts in table */
}contact_list_t;

typedef int (*insert_impurecord_t)(struct udomain* _d, str* public_identity, int reg_state, int barring,
        ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2,
        struct impurecord** _r);

typedef int (*get_impurecord_t)(struct udomain* _d, str* _aor, struct impurecord** _r);

typedef int (*delete_impurecord_t)(struct udomain* _d, str* _aor, struct impurecord* _r);

typedef int (*update_impurecord_t)(struct udomain* _d, str* public_identity, int reg_state, int send_sar_on_delete, int barring, int is_primary, ims_subscription** s, str* ccf1, str* ccf2, str* ecf1, str* ecf2, struct impurecord** _r);

typedef void (*lock_contact_slot_t)(str* contact_uri);

typedef void (*unlock_contact_slot_t)(str* contact_uri);

typedef void (*lock_contact_slot_i_t)(int sl);

typedef void (*unlock_contact_slot_i_t)(int sl);

typedef int (*update_ucontact_t)(struct impurecord* _r, struct ucontact* _c, struct ucontact_info* _ci);

typedef int (*expire_ucontact_t)(struct impurecord* _r, struct ucontact* _c);

typedef int (*unlink_contact_from_impu_t)(struct impurecord* _r, struct ucontact* _c, int write_to_db);

typedef int (*link_contact_to_impu_t)(struct impurecord* _r, struct ucontact* _c, int wirte_to_db);

typedef int (*insert_ucontact_t)(struct impurecord* _r, str* _contact, struct ucontact_info* _ci, struct ucontact** _c);

typedef int (*delete_ucontact_t)(struct ucontact* _c);

typedef int (*get_ucontact_t)(struct impurecord* _r, str* _c, str* _callid, str* _path, int _cseq, struct ucontact** _co);

typedef void (*release_ucontact_t)(struct ucontact* _c);

typedef int (*add_dialog_data_to_contact_t)(struct ucontact* _c, unsigned int h_entry, unsigned int h_id);

typedef int (*remove_dialog_data_from_contact_t)(struct ucontact* _c, unsigned int h_entry, unsigned int h_id);

typedef void (*lock_udomain_t)(struct udomain* _d, str *_aor);

typedef void (*unlock_udomain_t)(struct udomain* _d, str *_aor);

typedef int (*register_udomain_t)(const char* _n, struct udomain** _d);

typedef int (*get_all_ucontacts_t)(void* buf, int len, unsigned int flags, unsigned int part_idx, unsigned int part_max);

typedef int (*get_udomain_t)(const char* _n, udomain_t** _d);

typedef int (*update_subscriber_t)(impurecord_t* urec, reg_subscriber** _reg_subscriber,
        int *expires, int *local_cseq, int *version);

typedef void (*external_delete_subscriber_t)(reg_subscriber *s, udomain_t* _t, int lock_domain);

typedef int (*get_subscriber_t)(impurecord_t* urec, str *watcher_contact, str *presentity_uri, int event, reg_subscriber** reg_subscriber);

typedef int (*add_subscriber_t)(impurecord_t* urec,
		subscriber_data_t* subscriber_data, reg_subscriber** _reg_subscriber, int db_load);

typedef int (*get_impus_from_subscription_as_string_t)(udomain_t* _d, impurecord_t* impu_rec, int barring, str** impus, int* num_impus);

typedef str (*get_presentity_from_subscriber_dialog_t)(str *callid, str *to_tag, str *from_tag);

/*! usrloc API export structure */
typedef struct usrloc_api {
    int use_domain; /*! use_domain module parameter */
    int db_mode; /*! db_mode module parameter */
    unsigned int nat_flag; /*! nat_flag module parameter */

    register_udomain_t register_udomain;
    get_udomain_t get_udomain;
    lock_udomain_t lock_udomain;
    unlock_udomain_t unlock_udomain;

    insert_impurecord_t insert_impurecord;
    delete_impurecord_t delete_impurecord;
    get_impurecord_t get_impurecord;
    update_impurecord_t update_impurecord;

    lock_contact_slot_t lock_contact_slot;
    unlock_contact_slot_t unlock_contact_slot;
    lock_contact_slot_i_t lock_contact_slot_i;
    unlock_contact_slot_i_t unlock_contact_slot_i;
    insert_ucontact_t insert_ucontact;
    delete_ucontact_t delete_ucontact;
    get_ucontact_t get_ucontact;
    release_ucontact_t release_ucontact;
    get_all_ucontacts_t get_all_ucontacts;
    update_ucontact_t update_ucontact;
    expire_ucontact_t expire_ucontact;
    unlink_contact_from_impu_t unlink_contact_from_impu;
    link_contact_to_impu_t link_contact_to_impu;
    //update_user_profile_t update_user_profile;
    
    add_dialog_data_to_contact_t add_dialog_data_to_contact;
    remove_dialog_data_from_contact_t remove_dialog_data_from_contact;

    add_subscriber_t add_subscriber;
    update_subscriber_t update_subscriber;
    external_delete_subscriber_t external_delete_subscriber;
    get_subscriber_t get_subscriber;

    get_impus_from_subscription_as_string_t get_impus_from_subscription_as_string;

    register_ulcb_t register_ulcb;
    
    get_presentity_from_subscriber_dialog_t get_presentity_from_subscriber_dialog;
    
} usrloc_api_t;

/*! usrloc API export bind function */
typedef int (*bind_usrloc_t)(usrloc_api_t* api);

#endif
