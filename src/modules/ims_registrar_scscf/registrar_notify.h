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

#ifndef S_CSCF_REGISTRAR_NOTIFY_H_
#define S_CSCF_REGISTRAR_NOTIFY_H_


#include "../ims_usrloc_scscf/usrloc.h"
#include "../../core/locking.h"
#include "sem.h"


#define MSG_REG_SUBSCRIBE_OK "Subscription to REG saved"
#define MSG_REG_UNSUBSCRIBE_OK "Subscription to REG dropped"
#define MSG_REG_PUBLISH_OK "Publish to REG saved"

typedef struct _reg_notification {
    
    str subscription_state; /**< Subscription-state header value*/
    str content_type; /**< content type					*/
    
    str watcher_contact;
    str watcher_uri;
    str presentity_uri;
    
    struct udomain* _d;
    str* impus;
    int num_impus;
    
    unsigned int local_cseq;
    unsigned int reginfo_s_version;
    str call_id;
    str from_tag;
    str to_tag;
    str record_route;
    str sockinfo_str;
    
    str *explit_dereg_contact;
    int num_explit_dereg_contact;

    struct _reg_notification *next; /**< next notification in the list	*/
    struct _reg_notification *prev; /**< previous notification in the list	*/
} reg_notification;



/** Notification List Structure */
typedef struct {
    gen_lock_t *lock; /**< lock for notifications ops		*/
    reg_notification *head; /**< first notification in the list	*/
    reg_notification *tail; /**< last notification in the list	*/
    gen_sem_t *empty;
    int size;
} reg_notification_list;

/** Events for subscriptions */
typedef enum {
    IMS_EVENT_NONE, /**< Generic, no event					*/
    IMS_EVENT_REG /**< Registration event					*/
} IMS_Events_enum_t;

extern IMS_Events_enum_t IMS_Events;

/** Event types for "reg" to generated notifications after */
typedef enum {
    IMS_REGISTRAR_NONE, /**< no event - donothing 							*/
    IMS_REGISTRAR_SUBSCRIBE, /**< Initial SUBSCRIBE - just send all data - this should not be treated though */
    IMS_REGISTRAR_UNSUBSCRIBE, /**< Final UnSUBSCRIBE - just send a NOTIFY which will probably fail */
    IMS_REGISTRAR_SUBSCRIBE_EXPIRED, /**< The subscribe has expired 						*/

    //richard we only use contact reg, refresh, expired and unreg
    IMS_REGISTRAR_CONTACT_REGISTERED, /**< Registered with REGISTER						*/
    IMS_REGISTRAR_CONTACT_REFRESHED, /**< The expiration was refreshed					*/
    IMS_REGISTRAR_CONTACT_EXPIRED, /**< A contact has expired and will be removed		*/
    IMS_REGISTRAR_CONTACT_UNREGISTERED, /**< User unregistered with Expires 0				*/
    IMS_REGISTRAR_CONTACT_UNREGISTERED_IMPLICIT, /**< User unregistered implicitly, ie not via explicit deregister	*/
	IMS_REGISTRAR_SUBSEQUENT_SUBSCRIBE
} IMS_Registrar_events_enum_t;

extern IMS_Registrar_events_enum_t IMS_Registrar_events;

extern reg_notification_list *notification_list; //< List of pending notifications

int can_subscribe_to_reg(struct sip_msg *msg, char *str1, char *str2);

int subscribe_to_reg(struct sip_msg *msg, char *str1, char *str2);

int can_publish_reg(struct sip_msg *msg, char *str1, char *str2);

int publish_reg(struct sip_msg *msg, char *str1, char *str2);

int subscribe_reply(struct sip_msg *msg, int code, char *text, int *expires, str *contact);

int event_reg(udomain_t* _d, impurecord_t* r_passed, ucontact_t* c_passed, int event_type, str *presentity_uri, str *watcher_contact, str *contact_uri,
                str *explit_dereg_contact, int num_explit_dereg_contact);


str generate_reginfo_full(udomain_t* _t, str* impu_list, int new_subscription, str *explit_dereg_contact, str* watcher_contact, int num_explit_dereg_contact, unsigned int reginfo_version);

str get_reginfo_partial(impurecord_t *r, ucontact_t *c, int event_type, unsigned int reginfo_version);

void create_notifications(udomain_t* _t, impurecord_t* r_passed, ucontact_t* c_passed, str *presentity_uri, str *watcher_contact, str *contact_uri,
                            str* impus, int num_impus, int event_type, str *explit_dereg_contact, int num_explit_dereg_contact);

void notification_event_process();

void free_notification(reg_notification *n);

void send_notification(reg_notification * n);

void add_notification(reg_notification *n);

reg_notification* new_notification(str subscription_state,
        str content_type, str** impus, int num_impus, reg_subscriber* r, str **explit_dereg_contact, int num_explit_dereg_contact);

dlg_t* build_dlg_t_from_notification(reg_notification* n);


int notify_init();
void notify_destroy();

int aor_to_contact(str* aor, str* contact);
int contact_port_ip_match(str *c1, str *c2);

int notify_subscribers(impurecord_t* impurecord, ucontact_t* contact, str *explit_dereg_contact, int num_explit_dereg_contact, int event_type);

#endif //S_CSCF_REGISTRAR_NOTIFY_H_
