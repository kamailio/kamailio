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

#include "cdp_load.h"

#include "../../sr_module.h"


#define LOAD_ERROR "ERROR: cdp_bind: S-CSCF module function "

#define FIND_EXP(NAME) \
	if (!( cdpb->NAME=(NAME##_f) \
		find_export(#NAME, NO_SCRIPT, 0)) ) {\
		LM_ERR("'"LOAD_ERROR "'"#NAME"' not found\n");\
		return -1;\
	}


/**
 * Load the CDiameterPeer bindings
 * @param *cdpb - target structure to load the bindings into
 * @returns 1 on success, -1 on failure
 */  
int load_cdp( struct cdp_binds *cdpb)
{
	FIND_EXP(AAACreateRequest);
	FIND_EXP(AAACreateResponse);
	FIND_EXP(AAAFreeMessage);

	FIND_EXP(AAACreateAVP);
	FIND_EXP(AAAAddAVPToMessage);
	FIND_EXP(AAAAddAVPToList);
	FIND_EXP(AAAFindMatchingAVP);
	FIND_EXP(AAAFindMatchingAVPList);
	FIND_EXP(AAAGetNextAVP);
	FIND_EXP(AAAFreeAVP);
	FIND_EXP(AAAFreeAVPList);
	FIND_EXP(AAAGroupAVPS);
	FIND_EXP(AAAUngroupAVPS);

	FIND_EXP(AAASendMessage);
	FIND_EXP(AAASendMessageToPeer);
	FIND_EXP(AAASendRecvMessage);
	FIND_EXP(AAASendRecvMessageToPeer);

	FIND_EXP(AAAAddRequestHandler);
	FIND_EXP(AAAAddResponseHandler);

	FIND_EXP(AAACreateTransaction);
	FIND_EXP(AAADropTransaction);

	FIND_EXP(AAACreateSession);
	FIND_EXP(AAAMakeSession);
	FIND_EXP(AAAGetSession);
	FIND_EXP(AAADropSession);
	FIND_EXP(AAASessionsLock);
	FIND_EXP(AAASessionsUnlock);

	FIND_EXP(AAACreateClientAuthSession);
	FIND_EXP(AAACreateServerAuthSession);
	FIND_EXP(AAAGetAuthSession);
	FIND_EXP(AAADropAuthSession);
	FIND_EXP(AAATerminateAuthSession);
	
	/* Credit Control Application API - RFC 4006 */
	FIND_EXP(AAACreateCCAccSession);
	FIND_EXP(AAAStartChargingCCAccSession);
	FIND_EXP(AAAGetCCAccSession);
	FIND_EXP(AAADropCCAccSession);
	FIND_EXP(AAATerminateCCAccSession);

	return 1;
}
