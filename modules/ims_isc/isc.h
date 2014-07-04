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

#ifndef _ISC_ISC_H
#define _ISC_ISC_H

#include "../../parser/msg_parser.h"
#include "../../lib/ims/ims_getters.h"
#include "../../dset.h"
#include "mod.h"
#include "checker.h"
#include "mark.h"

#define IFC_AS_UNAVAILABLE_STATUS_CODE 555

extern struct tm_binds isc_tmb;		/**< Structure with pointers to tm funcs 		*/

extern str isc_my_uri;				/**< Uri of myself to loop the message in str	*/
extern str isc_my_uri_sip;			/**< Uri of myself to loop the message in str with leading "sip:" */

extern int isc_fr_timeout;			/**< default ISC response timeout in ms */
extern int isc_fr_inv_timeout;		/**< default ISC INVITE response timeout in ms */

/**	SIP Status Code to send to client on Session Termination because AS did not respond */


int isc_forward( struct sip_msg *msg, isc_match *m,isc_mark *mark);


#endif
