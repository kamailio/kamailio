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


#ifndef AUTHORIZE_H
#define AUTHORIZE_H


#include "../../parser/msg_parser.h"
#include "api.h"
#include "conversion.h"
#include "rfc2617.h"
#include "sip_messages.h"
#include "cxdx_mar.h"


#define NONCE_LEN 16
#define RAND_LEN  16



enum authorization_types {
	AUTH_UNKNOWN			= 0,
/* 3GPP */	
	AUTH_AKAV1_MD5			= 1,
	AUTH_AKAV2_MD5			= 2,
	AUTH_EARLY_IMS			= 3,
/* FOKUS */
	AUTH_MD5				= 4,
/* CableLabs */	
	AUTH_DIGEST				= 5,
/* 3GPP */	
	AUTH_SIP_DIGEST			= 6,
/* TISPAN */	
	AUTH_HTTP_DIGEST_MD5	= 7,	
	AUTH_NASS_BUNDLED		= 8
};

/** Enumeration for the Authorization Vector status */
enum auth_vector_status {
	AUTH_VECTOR_UNUSED = 0,
	AUTH_VECTOR_SENT = 1,
	AUTH_VECTOR_USELESS = 2,	/**< invalidated, marked for deletion 		*/
	AUTH_VECTOR_USED = 3		/**< the vector has been successfully used	*/
} ;

/** Authorization Vector storage structure */
typedef struct _auth_vector {
	int item_number;	/**< index of the auth vector		*/
	unsigned char type;	/**< type of authentication vector 	*/
	str authenticate;	/**< challenge (rand|autn in AKA)	*/
	str authorization; 	/**< expected response				*/
	str ck;				/**< Cypher Key						*/
	str ik;				/**< Integrity Key					*/
	time_t expires;/**< expires in (after it is sent)	*/
	uint32_t use_nb;		/**< number of use (nonce count)*/
	
	enum auth_vector_status status;/**< current status		*/
	struct _auth_vector *next;/**< next av in the list		*/
	struct _auth_vector *prev;/**< previous av in the list	*/
} auth_vector;

/** Set of auth_vectors used by a private id */
typedef struct _auth_userdata{
	unsigned int hash;		/**< hash of the auth data		*/
	str private_identity;	/**< authorization username		*/
	str public_identity;	/**< public identity linked to	*/
	time_t expires;			/**< expires in					*/

	auth_vector *head;		/**< first auth vector in list	*/
	auth_vector *tail;		/**< last auth vector in list	*/
	
	struct _auth_userdata *next;/**< next element in list	*/
	struct _auth_userdata *prev;/**< previous element in list*/
} auth_userdata;

/** Authorization user data hash slot */
typedef struct {
	auth_userdata *head;				/**< first in the slot			*/ 
	auth_userdata *tail;				/**< last in the slot			*/
	gen_lock_t *lock;			/**< slot lock 							*/	
} auth_hash_slot_t;



int auth_db_init(const str* db_url);
int auth_db_bind(const str* db_url);
void auth_db_close(void);

/*
 * Authorize using Proxy-Authorization header field
 */
int proxy_authenticate(struct sip_msg* _msg, char* _realm, char* _table);
int proxy_challenge(struct sip_msg* msg, char* route, char* _realm, char* str2);

/*
 * Authorize using WWW-Authorization header field
 */
int www_authenticate(struct sip_msg* _msg, char* _realm, char* _table);
int www_challenge2(struct sip_msg* msg, char* route, char* _realm, char* str2);
int www_challenge3(struct sip_msg* msg, char* route, char* _realm, char* str2);
int www_resync_auth(struct sip_msg* msg, char* _route, char* str1, char* str2);


/*
 * Bind to IMS_AUTH API
 */
int bind_ims_auth(ims_auth_api_t* api);

auth_vector* get_auth_vector(str private_identity,str public_identity,int status,str *nonce,unsigned int *hash);
/*
 * Storage of authentication vectors
 */

inline void auth_data_lock(unsigned int hash);
inline void auth_data_unlock(unsigned int hash);
 
int auth_data_init(int size);

void auth_data_destroy();

auth_vector *new_auth_vector(int item_number,str auth_scheme,str authenticate,
			str authorization,str ck,str ik);
void free_auth_vector(auth_vector *av);

auth_userdata *new_auth_userdata(str private_identity,str public_identity);
void free_auth_userdata(auth_userdata *aud);					

inline unsigned int get_hash_auth(str private_identity,str public_identity);

int add_auth_vector(str private_identity,str public_identity,auth_vector *av);
auth_vector* get_auth_vector(str private_identity,str public_identity,int status,str *nonce,unsigned int *hash);

int drop_auth_userdata(str private_identity,str public_identity);
auth_userdata* get_auth_userdata(str private_identity,str public_identity);

int stateful_request_reply(struct sip_msg *msg, int code,  char *text);
int stateful_request_reply_async(struct cell* t, struct sip_msg *msg, int code, char *text);

int multimedia_auth_request(struct sip_msg *msg, str public_identity, str private_identity,
					int count,str auth_scheme,str nonce,str auts,str servername, saved_transaction_t* transaction_data);
int pack_challenge(struct sip_msg *msg,str realm,auth_vector *av, int is_proxy_auth);
int add_authinfo_resp_hdr(struct sip_msg *msg, str nextnonce, str qop, HASHHEX rspauth, str cnonce, str nc);

inline void start_reg_await_timer(auth_vector *av);
void reg_await_timer(unsigned int ticks, void* param);

unsigned char get_algorithm_type(str algorithm);

#endif /* AUTHORIZE_H */
