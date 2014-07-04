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

#ifndef _ISC_CHECKER_H
#define _ISC_CHECKER_H

#include <sys/types.h> /* for regex */
#include <regex.h>
#include "../../sr_module.h"
#include "../../lib/ims/ims_getters.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_content.h"
#include "../../str.h"
#include "../../mem/mem.h"
#include "mod.h"

extern usrloc_api_t isc_ulb;/*!< Structure containing pointers to usrloc functions*/

#define TRUE 1
#define FALSE 0

/** ISC match structure */
typedef struct {
	str server_name;		/**< SIP URI of the AS to forward to */
	char default_handling;	/**< handling to apply on failure to contact the AS */
	str service_info;		/**< additional service information */
	int index;				/**< index of the matching IFC */
} isc_match;


/**
 * Find the next match and fill up the ifc_match structure with the position of the match
 * @param uri - URI of the user for which to apply the IFC
 * @param direction - direction of the session
 * @param skip - how many IFCs to skip because already matched
 * @param msg - the SIP initial request to check on 
 * @return - TRUE if found, FALSE if none found, end of search space 
 */
isc_match* isc_checker_find(str uri,char direction,int skip,struct sip_msg *msg,int registered, udomain_t *d);

void isc_free_match(isc_match *m);
int isc_is_registered(str *uri, udomain_t *d);

#endif
