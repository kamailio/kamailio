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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "session.h"
#include "diameter.h"
#include "config.h"
#include "authstatemachine.h"
#include "acctstatemachine.h"
#include "timer.h"
#include "globals.h"

extern dp_config *config;		/**< Configuration for this diameter peer 	*/

gen_lock_t *session_lock;		/**< lock for session operation */

int sessions_hash_size = 1024;	/**< the size of the session hash table		*/
cdp_session_list_t *sessions;	/**< the session hash table					*/

unsigned int *session_id1;		/**< counter for first part of the session id */
unsigned int *session_id2;		/**< counter for second part of the session id */

#define GRACE_DISCON_TIMEOUT	60	/**< 60 seconds for a DISCON acct session to hang around for before being cleaned up */


/**
 * Lock a hash table row
 */
inline void AAASessionsLock(unsigned int hash)
{
	if ( hash >=0 && hash < sessions_hash_size ){
		lock_get(sessions[hash].lock);
	}
	else {
		LM_ERR("AAASessionsLock: hash :%d out of range of sessions_hash_size: %d !\n", hash, sessions_hash_size);
	}
}

/**
 * Unlock a hash table row
 */
inline void AAASessionsUnlock(unsigned int hash)
{

	if ( hash >=0 && hash < sessions_hash_size ){
		lock_release(sessions[hash].lock);
	}
	else {
		LM_ERR("AAASessionsLock: hash :%d out of range of sessions_hash_size: %d !\n", hash, sessions_hash_size);
	}
}



/**
 * Free a session structure
 */
void free_session(cdp_session_t *x)
{
	if (x){
		if (x->id.s) shm_free(x->id.s);
		switch(x->type){
			case UNKNOWN_SESSION:
				if (x->u.generic_data){
					LM_ERR("free_session(): The session->u.generic_data should be freed and reset before dropping the session!"
						"Possible memory leak!\n");
				}
				break;
			case AUTH_CLIENT_STATEFULL:
				break;
			case AUTH_SERVER_STATEFULL:
				break;
			case ACCT_CC_CLIENT:
				break;
			default:
				LM_ERR("free_session(): Unknown session type %d!\n",x->type);
		}

		if(x->dest_host.s) shm_free(x->dest_host.s);
		if(x->dest_realm.s) shm_free(x->dest_realm.s);
                if (x->sticky_peer_fqdn_buflen && x->sticky_peer_fqdn.s) {
                    shm_free(x->sticky_peer_fqdn.s);
                }
		shm_free(x);
	}
}


/**
 * Initializes the session related structures.
 */
int cdp_sessions_init(int hash_size)
{
	int i;
	session_lock = lock_alloc();
	if (!session_lock){
		LOG_NO_MEM("lock",sizeof(gen_lock_t));
		goto error;
	}
	session_lock = lock_init(session_lock);
	sessions_hash_size=hash_size;

	sessions = shm_malloc(sizeof(cdp_session_list_t)*hash_size);
	if (!sessions){
		LOG_NO_MEM("shm",sizeof(cdp_session_list_t)*hash_size);
		goto error;
	}
	memset(sessions,0,sizeof(cdp_session_list_t)*hash_size);

	for(i=0;i<hash_size;i++){
		sessions[i].lock = lock_alloc();
		if (!sessions[i].lock){
			LOG_NO_MEM("lock",sizeof(gen_lock_t));
			goto error;
		}
		sessions[i].lock = lock_init(sessions[i].lock);
	}

	session_id1 = shm_malloc(sizeof(unsigned int));
	if (!session_id1){
		LOG_NO_MEM("shm",sizeof(unsigned int));
		goto error;
	}
	session_id2 = shm_malloc(sizeof(unsigned int));
	if (!session_id2){
		LOG_NO_MEM("shm",sizeof(unsigned int));
		goto error;
	}
	srand((unsigned int)time(0));
	*session_id1 = rand();
	*session_id1 <<= 16;
	*session_id1 += time(0)&0xFFFF;
	*session_id2 = 0;

	add_timer(1,0,cdp_sessions_timer,0);
	return 1;
error:
	return 0;
}

/**
 * Destroys the session related structures.
 */
int cdp_sessions_destroy()
{
	int i;
	cdp_session_t *n,*x;

	if (session_lock){
		lock_get(session_lock);
		lock_destroy(session_lock);
		lock_dealloc((void*)session_lock);
		session_lock=0;
	}
	for(i=0;i<sessions_hash_size;i++){
		AAASessionsLock(i);
		for(x = sessions[i].head; x; x = n){
			n = x->next;
			free_session(x);
		}
		lock_destroy(sessions[i].lock);
		lock_dealloc((void*)sessions[i].lock);
	}
	shm_free(sessions);

	shm_free(session_id1);
	shm_free(session_id2);
	return 1;
}




/**
 * Computes the hash for a string.
 * @param aor - the aor to compute the hash on
 * @param hash_size - value to % with
 * @returns the hash % hash_size
 */
inline unsigned int get_str_hash(str x,int hash_size)
{
#define h_inc h+=v^(v>>3)
   char* p;
   register unsigned v;
   register unsigned h;

   h=0;
   for (p=x.s; p<=(x.s+x.len-4); p+=4){
       v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
       h_inc;
   }
   v=0;
   for (;p<(x.s+x.len); p++) {
       v<<=8;
       v+=*p;
   }
   h_inc;

   h=((h)+(h>>11))+((h>>13)+(h>>23));
   return (h)%hash_size;
#undef h_inc
}

/**
 * Create a new session structure
 * @param id - the session id string, already allocated in shm
 * @param type - the session type
 * @returns the new cdp_session_t on success or 0 on failure
 */
cdp_session_t* cdp_new_session(str id,cdp_session_type_t type)
{
	cdp_session_t *x=0;

	x = shm_malloc(sizeof(cdp_session_t));
	if (!x){
		LOG_NO_MEM("shm",sizeof(cdp_session_t));
		goto error;
	}
	memset(x,0,sizeof(cdp_session_t));
	x->id = id;
	x->type = type;
	x->hash = get_str_hash(x->id,sessions_hash_size);
	return x;
error:
	return 0;
}

/**
 * Adds the session to the session list.
 * \note This returns with a lock, so unlock when done
 * @param x - the session to add
 */
void cdp_add_session(cdp_session_t *x)
{
//	unsigned int hash;
	if (!x) return;
//	hash = get_str_hash(x->id,sessions_hash_size);
//	x->hash = hash;
	LM_DBG("adding a session with id %.*s\n",x->id.len,x->id.s);
	AAASessionsLock(x->hash);
	x->next = 0;
	x->prev = sessions[x->hash].tail;
	if (sessions[x->hash].tail) sessions[x->hash].tail->next = x;
	sessions[x->hash].tail = x;
	if (!sessions[x->hash].head) sessions[x->hash].head = x;
}

/**
 * Finds a session in the session hash table.
 * \note Returns with a lock on the sessions[x->hash].lock!!!
 * @param id - the id of the session
 * @returns the session if found or 0 if not
 */
cdp_session_t* cdp_get_session(str id)
{
	unsigned int hash;
	cdp_session_t *x;
	if (!id.len) return 0;
	hash = get_str_hash(id,sessions_hash_size);
	LM_DBG("called get session with id %.*s and hash %u\n",id.len,id.s,hash);
	AAASessionsLock(hash);
		for(x = sessions[hash].head;x;x=x->next){
			LM_DBG("looking for |%.*s| in |%.*s|\n",id.len,id.s,x->id.len,x->id.s);
			if (x->id.len == id.len &&
				strncasecmp(x->id.s,id.s,id.len)==0)
					return x;
		}
	AAASessionsUnlock(hash);
	LM_DBG("no session found\n");
	return 0;
}


/**
 * Removes and frees a session.
 * \note must be called with a lock on the x->hash and it will unlock on exit. Do not use x after calling this
 *
 * @param x - the session to remove
 */
void del_session(cdp_session_t *x)
{
	unsigned int hash;

	if (!x) return;

	hash = x->hash;
	if (hash < 0 || hash >= sessions_hash_size) {
		LM_ERR("del_session: x->hash :%d out of range of sessions_hash_size: %d !\n",hash, sessions_hash_size);
		return;
	}

	if (sessions[x->hash].head == x) sessions[x->hash].head = x->next;
	else if (x->prev) x->prev->next = x->next;
	if (sessions[x->hash].tail == x) sessions[x->hash].tail = x->prev;
	else if (x->next) x->next->prev = x->prev;

	AAASessionsUnlock(hash);

	free_session(x);
}


/**
 * Generates a new session_ID (conforming with draft-ietf-aaa-diameter-17).
 * This function is thread safe.
 * @returns an 1 if success or -1 if error.
 */
static int generate_session_id(str *id, unsigned int end_pad_len)
{
	unsigned int s2;

	/* some checks */
	if (!id)
		goto error;

	/* compute id's len */
	id->len = config->identity.len +
		1/*;*/ + 10/*high 32 bits*/ +
		1/*;*/ + 10/*low 32 bits*/ +
//		1/*;*/ + 8/*optional value*/ +
		1 /* terminating \0 */ +
		end_pad_len;

	/* get some memory for it */
	id->s = (char*)shm_malloc( id->len );
	if (id->s==0) {
		LM_ERR("generate_session_id: no more free memory!\n");
		goto error;
	}

	lock_get(session_lock);
	s2 = *session_id2 +1;
	*session_id2 = s2;
	lock_release(session_lock);

	/* build the sessionID */
	sprintf(id->s,"%.*s;%u;%u",config->identity.len,config->identity.s,*session_id1,s2);
	id->len = strlen(id->s);
	return 1;
error:
	return -1;
}

void cdp_sessions_log()
{
	int hash;
	cdp_session_t *x;

	LM_DBG(ANSI_MAGENTA"------- CDP Sessions ----------------\n"ANSI_GREEN);
	for(hash=0;hash<sessions_hash_size;hash++){
		AAASessionsLock(hash);
		for(x = sessions[hash].head;x;x=x->next) {
			LM_DBG(ANSI_GRAY" %3u. [%.*s] AppId [%d] Type [%d]\n",
					hash,
					x->id.len,x->id.s,
					x->application_id,
					x->type);
			switch (x->type){
				case AUTH_CLIENT_STATEFULL:
				case AUTH_SERVER_STATEFULL:
					LM_DBG(ANSI_GRAY"\tAuth State [%d] Timeout [%d] Lifetime [%d] Grace [%d] Generic [%p]\n",
							x->u.auth.state,
							(int)(x->u.auth.timeout-time(0)),
							x->u.auth.lifetime?(int)(x->u.auth.lifetime-time(0)):-1,
							(int)(x->u.auth.grace_period),
							x->u.auth.generic_data);
					break;
				case ACCT_CC_CLIENT:
					LM_DBG(ANSI_GRAY"\tCCAcct State [%d] Charging Active [%c (%d)s] Reserved Units(valid=%ds) [%d] Generic [%p]\n",
							x->u.cc_acc.state,
							(x->u.cc_acc.charging_start_time&&x->u.cc_acc.state!=ACC_CC_ST_DISCON)?'Y':'N',
							x->u.cc_acc.charging_start_time?(int)((int)time(0) - (int)x->u.cc_acc.charging_start_time):-1,
							x->u.cc_acc.reserved_units?(int)((int)x->u.cc_acc.last_reservation_request_time + x->u.cc_acc.reserved_units_validity_time) - (int)time(0):-1,
							x->u.cc_acc.reserved_units,
							x->u.cc_acc.generic_data);
					break;
				default:
					break;
			}
		}
		AAASessionsUnlock(hash);
	}
	LM_DBG(ANSI_MAGENTA"-------------------------------------\n"ANSI_GREEN);
}

int cdp_sessions_timer(time_t now, void* ptr)
{
	int hash;
	cdp_session_t *x,*n;
	for(hash=0;hash<sessions_hash_size;hash++){
		AAASessionsLock(hash);
		for(x = sessions[hash].head;x;x=n) {
			n = x->next;
			switch (x->type){
				case ACCT_CC_CLIENT:
					if (x->u.cc_acc.type == ACC_CC_TYPE_SESSION) {
						//check for old, stale sessions, we need to do something more elegant
						//here to ensure that if a CCR start record is sent and the client never sends anything
						//else that we catch it and clean up the session from within CDP, calling all callbacks, etc
						if ((time(0) > (x->u.cc_acc.discon_time + GRACE_DISCON_TIMEOUT)) && (x->u.cc_acc.state==ACC_CC_ST_DISCON)) {
							cc_acc_client_stateful_sm_process(x, ACC_CC_EV_SESSION_STALE, 0);
						}
						//check reservation timers - again here we are assuming CC-Time applications
						int last_res_timestamp = x->u.cc_acc.last_reservation_request_time;
						int res_valid_for = x->u.cc_acc.reserved_units_validity_time;
						int last_reservation = x->u.cc_acc.reserved_units;
						int buffer_time = 15; //15 seconds - TODO: add as config parameter
						//we should check for reservation expiries if the state is open
						if(x->u.cc_acc.state==ACC_CC_ST_OPEN){
						    if (last_res_timestamp) {
							    //we have obv already started reservations
							    if ((last_res_timestamp + res_valid_for) < (time(0) + last_reservation + buffer_time)) {
								    LM_DBG("reservation about to expire, sending callback\n");
								    cc_acc_client_stateful_sm_process(x, ACC_CC_EV_RSVN_WARNING, 0);
							    }

						    }
						}
						/* TODO: if reservation has expired we need to tear down the session. Ideally 
						 * the client application (module) should do this but for completeness we should
						 * put a failsafe here too.
						 */
					}
					break;
				case AUTH_CLIENT_STATEFULL:
					if (x->u.auth.timeout>=0 && x->u.auth.timeout<=now){
						//Session timeout
						LM_CRIT("session TIMEOUT\n");
						auth_client_statefull_sm_process(x,AUTH_EV_SESSION_TIMEOUT,0);
					} else if (x->u.auth.lifetime>0 && x->u.auth.lifetime+x->u.auth.grace_period<=now){
						//lifetime + grace timeout
						LM_CRIT("lifetime+grace TIMEOUT\n");
						auth_client_statefull_sm_process(x,AUTH_EV_SESSION_GRACE_TIMEOUT,0);
					}else if (x->u.auth.lifetime>0 && x->u.auth.lifetime<=now){
						//lifetime timeout
						LM_CRIT("lifetime+grace TIMEOUT\n");
						auth_client_statefull_sm_process(x,AUTH_EV_SESSION_LIFETIME_TIMEOUT,0);
					}
					break;
				case AUTH_SERVER_STATEFULL:
					if (x->u.auth.timeout>=0 && x->u.auth.timeout<=now){
						//Session timeout
						LM_CRIT("session TIMEOUT\n");
						auth_server_statefull_sm_process(x,AUTH_EV_SESSION_TIMEOUT,0);
					}else if (x->u.auth.lifetime>0 && x->u.auth.lifetime+x->u.auth.grace_period<=now){
						//lifetime + grace timeout
						LM_CRIT("lifetime+grace TIMEOUT\n");
						auth_server_statefull_sm_process(x,AUTH_EV_SESSION_GRACE_TIMEOUT,0);
					}else if (x->u.auth.lifetime>0 && x->u.auth.lifetime<=now){
						//lifetime timeout
						LM_CRIT("lifetime+grace TIMEOUT\n");
						auth_server_statefull_sm_process(x,AUTH_EV_SESSION_LIFETIME_TIMEOUT,0);
					}
					break;
				default:
					break;

			}
		}
		AAASessionsUnlock(hash);
	}
	if (now%5==0)cdp_sessions_log();
	return 1;
}



/****************************** API FUNCTIONS ********************************/

/**
 * Creates a Generic Session.
 */
AAASession* AAACreateSession(void *generic_data)
{
	AAASession *s;
	str id;

	generate_session_id(&id,0);
	s = cdp_new_session(id,UNKNOWN_SESSION);
	if (s) {
		s->u.generic_data = generic_data;
		cdp_add_session(s);
	}
	return s;
}

/**
 * Make a session based on already known parameters.
 * \Note This should be used to recover saved sessions after a restart.
 * @param app_id
 * @param type
 * @param session_id - will be duplicated to shm
 * @return
 */
AAASession* AAAMakeSession(int app_id,int type,str session_id)
{
	AAASession *s;
	str id;

	id.s = shm_malloc(session_id.len);
	if (!id.s){
		LM_ERR("Error allocating %d bytes!\n",session_id.len);
		return 0;
	}
	memcpy(id.s,session_id.s,session_id.len);
	id.len = session_id.len;
	s = cdp_new_session(id,type);
	s->application_id = app_id;
	if (s) {
		cdp_add_session(s);
	}
	return s;
}
/**
 * Deallocates the memory taken by a Generic Session
 */
void AAADropSession(AAASession *s)
{
	// first give a chance to the cb to free the generic param
	if (s&&s->cb)
		(s->cb)(AUTH_EV_SESSION_DROP,s);
	del_session(s);
}


AAASession* cdp_new_auth_session(str id,int is_client,int is_statefull)
{
	AAASession *s;
	cdp_session_type_t type;

	if (is_client){
		if (is_statefull) type = AUTH_CLIENT_STATEFULL;
		else type = AUTH_CLIENT_STATELESS;
	}else{
		if (is_statefull) type = AUTH_SERVER_STATEFULL;
		else type = AUTH_SERVER_STATELESS;
	}
	s = cdp_new_session(id,type);
	if (s) {
		s->u.auth.timeout=time(0)+config->default_auth_session_timeout;
		s->u.auth.lifetime=0;
		s->u.auth.grace_period=0;
		cdp_add_session(s);
	}
	return s;
}

/**
 * Creates a Credit Control Accounting session for Client.
 * It generates a new id and adds the session to the cdp list of sessions
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* cdp_new_cc_acc_session(str id, int is_statefull)
{
	AAASession *s;
	cdp_session_type_t type;

	if (is_statefull) type = ACCT_CC_CLIENT;
	else type = ACCT_CC_CLIENT; //for now everything will be supported through this SM (until we add IEC)

	s = cdp_new_session(id,type);
	if (s) {
		if (is_statefull)
			s->u.cc_acc.type = ACC_CC_TYPE_SESSION;
		else
			s->u.cc_acc.type = ACC_CC_TYPE_EVENT;

//		s->u.cc_acc.timeout=time(0)+config->default_cc_acct_session_timeout;
		cdp_add_session(s);
	}
	return s;
}

/**
 * Creates a Authorization Session for the Client.
 * It generates a new id and adds the session to the cdp list of sessions
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAACreateClientAuthSession(int is_statefull,AAASessionCallback_f *cb,void *generic_data)
{
	AAASession *s;
	str id;

	generate_session_id(&id,0);

	s = cdp_new_auth_session(id,1,is_statefull);
	if (s) {
		s->u.auth.generic_data = generic_data;
		s->cb = cb;
		if (s->cb)
			(s->cb)(AUTH_EV_SESSION_CREATED,s);
	}
	return s;
}
/**
 * Creates a Authorization Session for the Server, from the application specific Session starting request
 * It generates a new id and adds the session to the cdp list of sessions
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAACreateServerAuthSession(AAAMessage *msg,int is_statefull,AAASessionCallback_f *cb,void *generic_data)
{
	AAASession *s;
	str id;

	if (!msg||!msg->sessionId||!msg->sessionId->data.len){
		LM_ERR("Error retrieving the Session-Id from the message.\n");
		return 0;
	}
	id.s = shm_malloc(msg->sessionId->data.len);
	if (!id.s){
		LM_ERR("Error allocating %d bytes of shm!\n",msg->sessionId->data.len);
		return 0;
	}else{
		id.len = msg->sessionId->data.len;
		memcpy(id.s,msg->sessionId->data.s,id.len);
		s=cdp_new_auth_session(id,0,is_statefull);
		if (s) {
			s->u.auth.generic_data = generic_data;
			s->cb = cb;
			if (s->cb)
				(s->cb)(AUTH_EV_SESSION_CREATED,s);
			update_auth_session_timers(&(s->u.auth),msg);
			auth_server_statefull_sm_process(s,AUTH_EV_RECV_REQ,msg);
			// this is a special exception where the session lock is not released
			//s=0;
		}
	}
	return s;
}

/**
 * Looks for a session with a given id and returns it if found
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAAGetSession(str id)
{
	return cdp_get_session(id);
}

/**
 * Looks for an Auth session with a given id and returns it if found
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAAGetAuthSession(str id)
{
	AAASession *x=cdp_get_session(id);
	if (x){
		switch (x->type){
			case AUTH_CLIENT_STATEFULL:
			case AUTH_CLIENT_STATELESS:
			case AUTH_SERVER_STATEFULL:
			case AUTH_SERVER_STATELESS:
				return x;
			default:
				AAASessionsUnlock(x->hash);
				return 0;
		}
	}
	return 0;
}

/**
 * Sends a Service terminated event to the session
 */
void AAATerminateAuthSession(AAASession *s)
{
	if (s->type==AUTH_CLIENT_STATEFULL) {
		auth_client_statefull_sm_process(s,AUTH_EV_SERVICE_TERMINATED,0);
	}
}

/**
 * Deallocates the memory taken by a Authorization Session
 * \note Must be called with a lock on the s->hash - will unlock it, so don't use the session after this
 */
void AAADropAuthSession(AAASession *s)
{
	AAADropSession(s);
}

/**
 * Creates an Accounting Session.
 */
AAASession* AAACreateAccSession(void *generic_data)
{
	return 0;
}

/**
 * Deallocates the memory taken by a Accounting Session
 */
void AAADropAccSession(AAASession *s)
{
	AAADropSession(s);
}

/**
 * Creates an Accounting Session (Credit control - RFC 4006) for the client
 * It generates a new id and adds the session to the cdp list of sessions
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAACreateCCAccSession(AAASessionCallback_f *cb, int is_session, void *generic_data)
{
	AAASession *s;
	str id;

	generate_session_id(&id, 0);

	s = cdp_new_cc_acc_session(id, is_session);
	if (s) {
		if (generic_data) 
			s->u.auth.generic_data = generic_data;
		s->cb = cb;
		if (s->cb)
			(s->cb)(ACC_CC_EV_SESSION_CREATED, s);
	}
	return s;
}

/**
 * Starts accounting on time-based CC App session (Credit control - RFC 4006) for the client
  * @returns 0 on success, anything else on failure
 */
int AAAStartChargingCCAccSession(AAASession *s)
{
	if (s->type != ACCT_CC_CLIENT && s->u.cc_acc.type != ACC_CC_TYPE_SESSION) {
		LM_ERR("Can't start charging on a credit-control session that is not session based\n");
		return -1;
	}

	s->u.cc_acc.charging_start_time = time(0);
	return 0;
}
/**
 * Deallocates the memory taken by a Accounting Session (Credit Control - RFC 4006)
 */
void AAADropCCAccSession(AAASession *s)
{
	AAADropSession(s);
}

/**
 * Looks for a CC Acc dession with a given id and returns it if found
 * \note Returns with a lock on AAASession->hash. Unlock when done working with the result
 * @returns the new AAASession or null on error
 */
AAASession* AAAGetCCAccSession(str id)
{
	AAASession *x=cdp_get_session(id);
	if (x){
		switch (x->type){
			case ACCT_CC_CLIENT:
				return x;
			default:
				AAASessionsUnlock(x->hash);
				return 0;
		}
	}
	return 0;
}

/**
 * Sends a Service terminated event to the session
 */
void AAATerminateCCAccSession(AAASession *s)
{
	if (s->type==ACCT_CC_CLIENT) {
		//TODO: run state machine for terminate event
	}
}

void cdp_session_cleanup(cdp_session_t* s, AAAMessage* msg) {
    // Here we should drop the session ! and free everything related to it
    // but the generic_data thing should be freed by the callback function registered
    // when the auth session was created
    AAASessionCallback_f *cb;

    LM_DBG("cleaning up session %.*s\n", s->id.len, s->id.s);
    switch (s->type) {
    	case ACCT_CC_CLIENT:
    		if (s->cb) {
    			cb = s->cb;
    			(cb)(ACC_CC_EV_SESSION_TERMINATED, s);
    		}
    		AAADropCCAccSession(s);
    		break;
    	case AUTH_CLIENT_STATEFULL:
    	case AUTH_CLIENT_STATELESS:
			if (s->cb) {
				cb = s->cb;
				(cb)(AUTH_EV_SERVICE_TERMINATED, s);
			}
			AAADropAuthSession(s);
			break;
    	default:
    		LM_WARN("asked to cleanup unknown/unhandled session type [%d]\n", s->type);
    		break;
    }

}

