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

#ifndef I_CSCF_SCSCF_LIST_H
#define I_CSCF_SCSCF_LIST_H

#include "../../sr_module.h"
#include "../../modules/tm/tm_load.h"
#include "mod.h"
#ifndef __OS_darwin
#include <values.h>
#endif
#include "../../mem/shm_mem.h"

#include "../../dset.h"

#include "../../timer.h"

#ifdef __OS_darwin
#ifndef MAXINT
#define MAXINT INT_MAX
#endif
#endif


/** S-CSCF list element */ 
typedef struct _scscf_entry {
	str scscf_name;	/**< SIP URI of the S-CSCF */
	int score;		/**< score of the match */
	time_t start_time;
        
	struct _scscf_entry *next; /**< next S-CSCF in the list */
} scscf_entry;

/** S-CSCF list */
typedef struct _scscf_list {
	str call_id;			/**< Call-Id from the request */
	scscf_entry *list;		/**< S-CSCF list */
	
	struct _scscf_list *next;	/**< Next S-CSCF list in the hash slot */
	struct _scscf_list *prev;	/**< Previous S-CSCF list in the hash slot */
} scscf_list;

/** hash slot for S-CSCF lists */
typedef struct {
	scscf_list *head;					/**< first S-CSCF list in this slot */
	scscf_list *tail;					/**< last S-CSCF list in this slot */
	gen_lock_t *lock;				/**< slot lock 					*/	
} i_hash_slot;


/** S-CSCF with attached capabilities */
typedef struct _scscf_capabilities {
	int id_s_cscf;					/**< S-CSCF id in the DB */
	str scscf_name;					/**< S-CSCF SIP URI */
	int *capabilities;				/**< S-CSCF array of capabilities*/
	int cnt;						/**< size of S-CSCF array of capabilities*/
} scscf_capabilities;


extern struct tm_binds tmb;

/**
 * Initialize the hash with S-CSCF lists
 */
int i_hash_table_init(int hash_size);

/**
 * Frees memory for scscf list
 */
void free_scscf_list(scscf_list *sl);

/**
 * Returns a list of S-CSCFs that we should try on, based on the
 * capabilities requested
 * \todo - order the list according to matched optionals -
 * @param scscf_name - the first S-CSCF if specified
 * @param m - mandatory capabilities list
 * @param mcnt - mandatory capabilities list size
 * @param o - optional capabilities list
 * @param ocnt - optional capabilities list size
 * @param orig - indicates originating session case
 * @returns list of S-CSCFs, terminated with a str={0,0}
 */
scscf_entry* I_get_capab_ordered(str scscf_name,int *m,int mcnt,int *o,int ocnt, str *p, int pcnt,int orig);

/**
 * Creates new scscf entry structure
 */
scscf_entry* new_scscf_entry(str name, int score, int orig);

/**
 * Returns the matching rank of a S-CSCF
 * \todo - optimize the search as O(n^2) is hardly desireable
 * @param c - the capabilities of the S-CSCF
 * @param m - mandatory capabilities list requested
 * @param mcnt - mandatory capabilities list size
 * @param o - optional capabilities list
 * @param ocnt - optional capabilities list sizeint I_get_capab_match(ims_icscf_capabilities *c,int *m,int mcnt,int *o,int ocnt)
 * @returns - -1 if mandatory not satisfied, else count of matched optional capab
 */
int I_get_capabilities();
int I_get_capab_match(scscf_capabilities *c,int *m,int mcnt,int *o,int ocnt);
int add_scscf_list(str call_id,scscf_entry *sl);
scscf_list* new_scscf_list(str call_id,scscf_entry *sl);
inline unsigned int get_call_id_hash(str callid,int hash_size);
inline void i_lock(unsigned int hash);
inline void i_unlock(unsigned int hash);
int I_scscf_select(struct sip_msg* msg, char* str1, char* str2);

/**
 * Takes on S-CSCF name for the respective Call-ID from the respective name list.
 * Don't free the result.s - it is freed later!
 * @param call_id - the id of the call
 * @returns the shm_malloced S-CSCF name if found or empty string if list is empty or does not exists 
 */
str take_scscf_entry(str call_id);
int I_scscf_drop(struct sip_msg* msg, char* str1, char* str2);
void del_scscf_list(str call_id);
void print_scscf_list(int log_level);

/**
 * Transactional SIP response - tries to create a transaction if none found.
 * @param msg - message to reply to
 * @param code - the Status-code for the response
 * @param text - the Reason-Phrase for the response
 * @returns the tmb.t_repy() result
 */
int cscf_reply_transactional(struct sip_msg *msg, int code, char *text);
int cscf_reply_transactional_async(struct cell* t, struct sip_msg *msg, int code, char *text);

/**
 * Timeout routine called every x seconds and determines if scscf_list entries should be expired
 * @param msg - message to reply to
 * @param code - the Status-code for the response
 * @param text - the Reason-Phrase for the response
 * @returns the tmb.t_repy() result
 */

void ims_icscf_timer_routine();

#endif
