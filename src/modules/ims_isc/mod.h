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

#ifndef ISC_MOD_H
#define ISC_MOD_H

#include "../../sr_module.h"
#include "../../modules/tm/tm_load.h"
#include "../../qvalue.h"
#include "../ims_usrloc_scscf/usrloc.h"

#define STR_APPEND(dst,src)\
        {memcpy((dst).s+(dst).len,(src).s,(src).len);\
        (dst).len = (dst).len + (src).len;}

/** SER routing script return and break execution */
#define ISC_RETURN_BREAK	 0
/** SER routing script return true */
#define ISC_RETURN_TRUE		 1
/** SER routing script return false */
#define ISC_RETURN_FALSE	-1
/** retargeting has happened */
#define ISC_RETURN_RETARGET	-2
/** SER routing script return error */
#define ISC_RETURN_ERROR 	-3

/** Direction of the dialog */
enum dialog_direction {
	DLG_MOBILE_ORIGINATING = 0, /** Originating */
	DLG_MOBILE_TERMINATING = 1, /** Terminating */
	DLG_MOBILE_UNKNOWN = 2
/** Unknown 	*/
};

/* Various constants */
/** User Not Registered */
#define IMS_USER_NOT_REGISTERED 0
/** User registered */
#define IMS_USER_REGISTERED 1
/** User unregistered (not registered but with services for unregistered state) */
#define IMS_USER_UNREGISTERED -1

#endif /* ISC_MOD_H */
