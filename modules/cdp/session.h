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

#ifndef __DIAMETER_SESSION_H
#define __DIAMETER_SESSION_H


//#include "diameter_api.h"
#include "utils.h"
#include "diameter.h"

/** Function for callback on session events: timeout, etc. */
typedef void (AAASessionCallback_f)(int event, void *session);

/** Types of sessions */
typedef enum {
	UNKNOWN_SESSION			= 0,

	AUTH_CLIENT_STATELESS	= 1,
	AUTH_SERVER_STATELESS	= 2,
	AUTH_CLIENT_STATEFULL	= 3,
	AUTH_SERVER_STATEFULL	= 4,

	ACCT_CLIENT_STATELESS	= 5,
	ACCT_CLIENT_STATEFUL	= 6,
	ACCT_SERVER_STATELESS	= 7,
	ACCT_SERVER_STATEFULL	= 8,

	ACCT_CC_CLIENT			= 9,		/**< Credit Control Client - RFC (4006) */

} cdp_session_type_t;

/** auth session states */
typedef enum {
	AUTH_ST_IDLE,
	AUTH_ST_PENDING,
	AUTH_ST_OPEN,
	AUTH_ST_DISCON
} cdp_auth_state;

/** auth session event */
typedef enum {
	AUTH_EV_START						=0,
	AUTH_EV_SEND_REQ 					=1,
	AUTH_EV_SEND_ANS					=2,
	AUTH_EV_SEND_ANS_SUCCESS			=3,
	AUTH_EV_SEND_ANS_UNSUCCESS			=4,
	AUTH_EV_RECV_ASR					=5,
	AUTH_EV_RECV_REQ					=6,
	AUTH_EV_RECV_ANS					=7,
	AUTH_EV_RECV_ANS_SUCCESS			=8,
	AUTH_EV_RECV_ANS_UNSUCCESS			=9,
	AUTH_EV_SEND_ASR					=10,
	AUTH_EV_SEND_ASA_SUCCESS			=11,
	AUTH_EV_SEND_ASA_UNSUCCESS			=12,
	AUTH_EV_SEND_STA					=13,
	AUTH_EV_RECV_ASA					=14,
	AUTH_EV_RECV_ASA_SUCCESS			=15,
	AUTH_EV_RECV_ASA_UNSUCCESS			=16,
	AUTH_EV_RECV_STA					=17,
	AUTH_EV_RECV_STR					=18,
	AUTH_EV_SESSION_LIFETIME_TIMEOUT	=19,
	AUTH_EV_SESSION_GRACE_TIMEOUT		=20,
	AUTH_EV_SESSION_TIMEOUT				=21,
	AUTH_EV_SERVICE_TERMINATED			=22,
	AUTH_EV_SESSION_CREATED				=23,
	AUTH_EV_SESSION_MODIFIED			=24,
	AUTH_EV_SESSION_DROP				=25,
} cdp_auth_event;

/** structure for auth session */
typedef struct _cdp_auth_session_t {
	cdp_auth_state state;	/**< current state */

	time_t timeout;			/**< absolute time for session timeout  -1 means forever */
	time_t lifetime;		/**< absolute time for auth lifetime -1 means forever */
	time_t grace_period;	/**< grace_period in seconds 	*/
	void* generic_data;
} cdp_auth_session_t;

/** Accounting states definition */
typedef enum {
	ACC_ST_IDLE			= 0,	/**< Idle */
	ACC_ST_PENDING_S	= 1,	/**< Pending Session */
	ACC_ST_PENDING_E	= 2,	/**< Pending Event */
	ACC_ST_PENDING_B	= 3,	/**< Pending Buffered */
	ACC_ST_OPEN	  		= 4,	/**< Open */
	ACC_ST_PENDING_I	= 5,	/**< Pending Interim */
	ACC_ST_PENDING_L	= 6		/**< PendingL - sent accounting stop */
} cdp_acc_state_t;

/** Accounting events definition */
typedef enum {
	ACC_EV_START					= 101,	/**< Client or device "requests access" (SIP session establishment) */
	ACC_EV_EVENT					= 102,	/**< Client or device requests a one-time service (e.g. SIP MESSAGE) */
	ACC_EV_BUFFEREDSTART			= 103,	/**< Records in storage */
	ACC_EV_RCV_SUC_ACA_START		= 104,	/**< Successful accounting start answer received */
	ACC_EV_SNDFAIL					= 105,	/**< Failure to send */
	ACC_EV_RCV_FAILED_ACA_START		= 106,	/**< Failed accounting start answer received */
	ACC_EV_STOP						= 107,	/**< User service terminated */
	ACC_EV_INTERIM					= 108,	/**< Interim interval elapses */
	ACC_EV_RCV_SUC_ACA_INTERIM		= 109,	/**< Successful accounting interim answer received */
	ACC_EV_RCV_FAILED_ACA_INTERIM	= 110,	/**< Failed accounting interim answer received */
	ACC_EV_RCV_SUC_ACA_EVENT		= 111,	/**< Successful accounting event answer received */
	ACC_EV_RCV_FAILED_ACA_EVENT		= 112,	/**< Failed accounting event answer received */
	ACC_EV_RCV_SUC_ACA_STOP			= 113,	/**< Successful accounting stop answer received */
	ACC_EV_RCV_FAILED_ACA_STOP		= 114,	/**< Failed accounting stop answer received */
} cdp_acc_event_t;

/** Credit Control states - RFC 4006 */
typedef enum {
	ACC_CC_ST_IDLE		= 0, 	/**< Idle */
	ACC_CC_ST_PENDING_I	= 1,	/**< Pending Initial Answer */
	ACC_CC_ST_PENDING_U	= 2,	/**< Pending Interim/Update Answer */
	ACC_CC_ST_PENDING_T	= 3,	/**< Pending Terminate Answer */
	ACC_CC_ST_OPEN		= 4,	/**< Open */
	ACC_CC_ST_DISCON	= 5,	/**< Disconnected, ie. end of session */
} cdp_cc_acc_state_t;

/** Credit Control events - RFC 4006 */
typedef enum {
	ACC_CC_EV_SESSION_START				= 0, 		/**< Client or device requests access to service */
	ACC_CC_EV_SEND_REQ					= 1,		/**< Sent initial request on session CCR */
	ACC_CC_EV_RECV_ANS					= 2,		/**< Received a response message */
	ACC_CC_EV_RECV_ANS_SUCCESS 			= 3,		/**< CCA = success */
	ACC_CC_EV_RECV_ANS_UNSUCCESS 		= 4,		/**< CCA = failure */
	ACC_CC_EV_SESSION_CREATED 			= 5,		/**< used for callbacks - new CC session created */
	ACC_CC_EV_SESSION_TIMEOUT			= 6,		/**< CC Session timeout */
	ACC_CC_EV_RSVN_WARNING				= 7,		/**< Reservation is about to expire */
	ACC_CC_EV_SESSION_TERMINATED		= 8,
	ACC_CC_EV_SESSION_MODIFIED			= 9,
	ACC_CC_EV_SESSION_STALE				= 10,		/**< Session is being removed from memory after stale expiry */
} cdp_cc_acc_event_t;

/** Credit Control Session types - RFC 4006 */
typedef enum {
	ACC_CC_TYPE_EVENT		= 0, 	/**< Immediate Event Charge (IEC - see TS32.299 6.3.2.1)*/
	ACC_CC_TYPE_SESSION		= 1, 	/**< Session-based charging (SCUR/ECUR - see TS32.299 6.3.2.1)*/
} cdp_cc_acc_type_t;

/** Credit Control Failure Handling */
typedef enum {
	ACC_CC_CCFH_UNKNOWN				= 0,
	ACC_CC_CCFH_CONTINUE 			= 1,
	ACC_CC_CCFH_TERMINATE			= 2,
	ACC_CC_CCFH_RETRY_AND_TERMINATE	= 3,
} cdp_cc_acc_ccfh_t;

/** Credit Control Final Unit Action */
typedef enum {
	ACC_CC_FUI_UNKNOWN				= 0,
	ACC_CC_FUI_TERMINATE			= 1,
} cdp_cc_acc_fua_t;

/** Structure for accounting sessions */
typedef struct _acc_session {
	cdp_acc_state_t state;					/**< current state */
	str dlgid;       						/**< application-level identifier, combines application session (e.g. SIP dialog) or event with diameter accounting session */
	unsigned int acct_record_number; 		/**< number of last accounting record within this session */
	time_t aii;	 							/**< expiration of Acct-Interim-Interval (seconds) */
	time_t timeout;							/**< session timeout (seconds) */
	void* generic_data;
} cdp_acc_session_t;

/** Structure for credit control accounting sessions (RFC 4006)*/
typedef struct _cc_acc_session {
	cdp_cc_acc_state_t state;				/**< current state */
	time_t discon_time;						/**< the time we went into DISCON state. we use this to cleanup dead sessions*/
	cdp_cc_acc_type_t type;					/**< type of CC, event or session (each have a different state machine - see RFC 4006 */
	cdp_cc_acc_ccfh_t ccfh;					/**< credit control failure handling type set for this session */
//	cdp_cc_acc_fua_t fua;					/**< final unit action */
	int fua;								/**< TODO: clean this up */
	unsigned int acct_record_number; 		/**< number of last accounting record within this session */
	time_t timeout;							/**< session timeout (seconds) - this is to prevent idle sessions from 'hanging' around */
	time_t charging_start_time;				/**< the time the session started to charge */
	time_t last_reservation_request_time;	/**< the last time we requested reservation for the current granted units */
	int reserved_units;						/**< current number of granted units */
	int reserved_units_validity_time;		/**< number of seconds current granted units valid for */

	void* generic_data;
} cdp_cc_acc_session_t;

/** Structure for session identification */
typedef struct _cdp_session_t {
	unsigned int hash;
	str id;                             /**< session-ID as string */
	unsigned int application_id;		/**< specific application id associated with this session */
	unsigned int vendor_id;				/**< specific vendor id for this session */
	cdp_session_type_t type;
	str dest_host, dest_realm; 			/*the destination host and realm, used only for auth, for the moment*/
        str sticky_peer_fqdn;                           /*peer that we would like to stick for for this session*/
        int sticky_peer_fqdn_buflen;                     /*length of buffer available for sticky peer*/
	union {
		cdp_auth_session_t auth;
		cdp_acc_session_t acc;
		cdp_cc_acc_session_t cc_acc;
		void *generic_data;
	} u;

	AAASessionCallback_f *cb;			/**< session callback function */

	struct _cdp_session_t *next,*prev;
} cdp_session_t;

/** Session list structure */
typedef struct _cdp_session_list_t {
	gen_lock_t *lock;				/**< lock for list operations */
	cdp_session_t *head,*tail;		/**< first, last sessions in the list */
} cdp_session_list_t;

int cdp_sessions_init(int hash_size);
int cdp_sessions_destroy();
void cdp_sessions_log();
int cdp_sessions_timer(time_t now, void* ptr);

void cdp_session_cleanup(cdp_session_t* s, AAAMessage* msg);	/**< Session cleanup for all session types */


cdp_session_t* cdp_get_session(str id);
cdp_session_t* cdp_new_session(str id,cdp_session_type_t type); //this function is needed in the peerstatemachine

void cdp_add_session(cdp_session_t *x);
cdp_session_t* cdp_new_auth_session(str id,int is_client,int is_statefull);
cdp_session_t* cdp_new_cc_acc_session(str id, int is_statefull);	//new redit control acct session

/*           API Exported */
typedef cdp_session_t AAASession;

AAASession* AAACreateSession(void *generic_data);
typedef AAASession* (*AAACreateSession_f)(void *generic_data);

AAASession* AAAMakeSession(int app_id,int type,str session_id);
typedef AAASession* (*AAAMakeSession_f)(int app_id,int type,str session_id);

AAASession* AAAGetSession(str id);
typedef AAASession* (*AAAGetSession_f)(str id);

void AAADropSession(AAASession *s);
typedef void (*AAADropSession_f)(AAASession *s);

void AAASessionsUnlock(unsigned int hash);
typedef void (*AAASessionsUnlock_f) (unsigned int hash);

void AAASessionsLock(unsigned int hash);
typedef void (*AAASessionsLock_f) (unsigned int hash);

AAASession* AAACreateClientAuthSession(int is_statefull,AAASessionCallback_f *cb,void *generic_data);
typedef AAASession* (*AAACreateClientAuthSession_f)(int is_statefull,AAASessionCallback_f *cb,void *generic_data);

AAASession* AAACreateServerAuthSession(AAAMessage *msg,int is_statefull,AAASessionCallback_f *cb,void *generic_data);
typedef AAASession* (*AAACreateServerAuthSession_f)(AAAMessage *msg,int is_statefull,AAASessionCallback_f *cb,void *generic_data);

AAASession* AAAGetAuthSession(str id);
typedef AAASession* (*AAAGetAuthSession_f)(str id);

void AAADropAuthSession(AAASession *s);
typedef void (*AAADropAuthSession_f)(AAASession *s);

void AAATerminateAuthSession(AAASession *s);
typedef void (*AAATerminateAuthSession_f)(AAASession *s);

AAASession* AAAGetCCAccSession(str id);
typedef AAASession* (*AAAGetCCAccSession_f)(str id);

AAASession* AAACreateAccSession(void *generic_data);
void AAADropAccSession(AAASession *s);

AAASession* AAACreateCCAccSession(AAASessionCallback_f *cb, int is_session, void *generic_data);	/*create CreditControl accounting session*/
typedef AAASession* (*AAACreateCCAccSession_f)(AAASessionCallback_f *cb, int is_session, void *generic_data);

int AAAStartChargingCCAccSession(AAASession *s);
typedef int (*AAAStartChargingCCAccSession_f)(AAASession *s);

void AAADropCCAccSession(AAASession *s);		/*drop CreditControl accounting session*/
typedef void (*AAADropCCAccSession_f)(AAASession *s);

void AAATerminateCCAccSession(AAASession *s);
typedef void (*AAATerminateCCAccSession_f)(AAASession *s);

#endif
