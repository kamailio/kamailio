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

#ifndef __TRANSACTION_H_
#define __TRANSACTION_H_

#include <time.h>
#include "utils.h"
#include "diameter.h"
#include "diameter_api.h"

/** Diameter Transaction representation */
typedef struct _cdp_trans_t{
	struct timeval started;			/**< Time the transaction was created - used to measure response times */
	AAAMsgIdentifier endtoendid;	/**< End-to-end id of the messages */
	AAAMsgIdentifier hopbyhopid;	/**< Hop-by-hop id of the messages */
	AAATransactionCallback_f *cb;	/**< transactional callback function */
	void **ptr;						/**< generic pointer to pass to the callback */
	AAAMessage *ans;				/**< answer for the transaction */
	time_t expires;					/**< time of expiration, when a time-out event will happen */
	int auto_drop;					/**< if to drop automatically the transaction on event or to let the app do it later */
	struct _cdp_trans_t *next;		/**< the next transaction in the transaction list */
	struct _cdp_trans_t *prev;		/**< the previous transaction in the transaction list */
} cdp_trans_t;

/** Diameter Transaction list */
typedef struct {		
	gen_lock_t *lock;				/**< lock for list operations */
	cdp_trans_t *head,*tail;		/**< first, last transactions in the list */ 
} cdp_trans_list_t;

int cdp_trans_init();
int cdp_trans_destroy();

inline cdp_trans_t* cdp_add_trans(AAAMessage *msg,AAATransactionCallback_f *cb, void *ptr,int timeout,int auto_drop);
void del_trans(AAAMessage *msg);
inline cdp_trans_t* cdp_take_trans(AAAMessage *msg);
inline void cdp_free_trans(cdp_trans_t *x);

int cdp_trans_timer(time_t now, void* ptr);

/*            API Exported    */

/** Timeout for Diameter transactions (this is quite big, 
 * but increase in case that you have a slow peer) */ 				

AAATransaction *AAACreateTransaction(AAAApplicationId app_id,AAACommandCode cmd_code);
typedef AAATransaction * (*AAACreateTransaction_f)(AAAApplicationId app_id,AAACommandCode cmd_code);

int AAADropTransaction(AAATransaction *trans);
typedef int (*AAADropTransaction_f)(AAATransaction *trans);



#endif /*TRANSACTION_H_*/
