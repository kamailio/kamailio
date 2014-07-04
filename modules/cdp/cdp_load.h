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

#ifndef _CDP_BIND_H
#define _CDP_BIND_H

#include "../../sr_module.h"
#include "utils.h"
#include "diameter.h"
#include "diameter_api.h"
#include "diameter_ims.h"
#include "diameter_epc.h"
#include "session.h"
//#include "peer.h"


struct cdp_binds {
	AAACreateRequest_f			AAACreateRequest;
	AAACreateResponse_f			AAACreateResponse;	
	AAAFreeMessage_f			AAAFreeMessage;
	
	
	AAACreateAVP_f				AAACreateAVP;
	AAAAddAVPToMessage_f		AAAAddAVPToMessage;
	AAAAddAVPToList_f			AAAAddAVPToList;
	AAAFindMatchingAVP_f		AAAFindMatchingAVP;
	AAAFindMatchingAVPList_f	AAAFindMatchingAVPList;
	AAAGetNextAVP_f				AAAGetNextAVP;
	AAAFreeAVP_f				AAAFreeAVP;
	AAAFreeAVPList_f			AAAFreeAVPList;
	AAAGroupAVPS_f				AAAGroupAVPS;
	AAAUngroupAVPS_f			AAAUngroupAVPS;

	AAASendMessage_f			AAASendMessage;
	AAASendMessageToPeer_f		AAASendMessageToPeer;
	AAASendRecvMessage_f		AAASendRecvMessage;
	AAASendRecvMessageToPeer_f	AAASendRecvMessageToPeer;
	
	
	AAAAddRequestHandler_f		AAAAddRequestHandler;
	AAAAddResponseHandler_f		AAAAddResponseHandler;


	AAACreateTransaction_f		AAACreateTransaction;
	AAADropTransaction_f		AAADropTransaction;
	
	
	AAACreateSession_f			AAACreateSession;
	AAAMakeSession_f			AAAMakeSession;
	AAAGetSession_f				AAAGetSession;
	AAADropSession_f			AAADropSession;
	AAASessionsLock_f 			AAASessionsLock;
	AAASessionsUnlock_f			AAASessionsUnlock;

	AAACreateClientAuthSession_f AAACreateClientAuthSession;
	AAACreateServerAuthSession_f AAACreateServerAuthSession;
	AAAGetAuthSession_f			AAAGetAuthSession;
	AAADropAuthSession_f		AAADropAuthSession;
	AAATerminateAuthSession_f	AAATerminateAuthSession;

	AAACreateCCAccSession_f 	AAACreateCCAccSession;
	AAAStartChargingCCAccSession_f	AAAStartChargingCCAccSession;
	AAAGetCCAccSession_f		AAAGetCCAccSession;
	AAADropCCAccSession_f		AAADropCCAccSession;
	AAATerminateCCAccSession_f	AAATerminateCCAccSession;

};


#define NO_SCRIPT	-1

typedef int(*load_cdp_f)( struct cdp_binds *cdpb );
int load_cdp( struct cdp_binds *cdpb);

static inline int load_cdp_api(struct cdp_binds* cdpb)
{
	load_cdp_f load_cdp;

	/* import the TM auto-loading function */
	load_cdp = (load_cdp_f)find_export("load_cdp", NO_SCRIPT, 0);
	
	if (load_cdp == NULL) {
		LM_WARN("Cannot import load_cdp function from CDP module\n");
		return -1;
	}
	
	/* let the auto-loading function load all TM stuff */
	if (load_cdp(cdpb) == -1) {
		return -1;
	}
	return 0;
}


#endif

