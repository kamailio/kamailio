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
 * History:
 * ---------
 */

/*! \file
 *  \brief USRLOC - Usrloc module interface
 *  \ingroup usrloc
 */

#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../lib/srdb1/db.h"
#include "../../str.h"


/*
 * Module parameters
 */


#define UL_TABLE_VERSION 1004

#include "../../lib/ims/useful_defs.h"
#include "../presence/event_list.h"
#include "../presence/hash.h"

extern int timer_interval;
extern int desc_time_order;
extern int cseq_delay;
extern int ul_fetch_rows;
extern int ul_hash_size;

/* functions imported from presence to handle subscribe hash table */
extern new_shtable_t pres_new_shtable;
extern insert_shtable_t pres_insert_shtable;
extern search_shtable_t pres_search_shtable;
extern update_shtable_t pres_update_shtable;
extern delete_shtable_t pres_delete_shtable;
extern destroy_shtable_t pres_destroy_shtable;
extern extract_sdialog_info_t pres_extract_sdialog_info;

/*
 * Matching algorithms
 */
#define CONTACT_ONLY            (0)
#define CONTACT_CALLID          (1)
#define CONTACT_PATH		(2)
#define CONTACT_PORT_IP_ONLY    (3)

extern int matching_mode;


#endif /* UL_MOD_H */
