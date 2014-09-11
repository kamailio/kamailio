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

#ifndef __DIAMETER_API_H_
#define __DIAMETER_API_H_

#include "diameter.h"
#include "utils.h"
#include "session.h"
#include "transaction.h"


/* MESSAGE CREATION */
				
AAAMessage *AAACreateRequest(AAAApplicationId app_id,
							AAACommandCode command_code,
							AAAMsgFlag flags,
							AAASession *session);
typedef AAAMessage* (*AAACreateRequest_f)(AAAApplicationId app_id,
							AAACommandCode command_code,
							AAAMsgFlag flags,
							AAASession *session);

AAAMessage *AAACreateResponse(AAAMessage *request);
typedef AAAMessage* (*AAACreateResponse_f)(AAAMessage *request);


AAAMessage *AAANewMessage(
		AAACommandCode commandCode,
		AAAApplicationId appId,
		AAASession *session,
		AAAMessage *request);

AAAReturnCode AAAFreeAVPList(AAA_AVP_LIST *avpList);
typedef AAAReturnCode  (*AAAFreeAVPList_f)(AAA_AVP_LIST *avpList);



AAAResultCode AAASetMessageResultCode(AAAMessage *message,AAAResultCode resultCode);

void AAAPrintMessage(AAAMessage *msg);

AAAReturnCode AAABuildMsgBuffer(AAAMessage *msg );

AAAMessage* AAATranslateMessage(unsigned char* source,unsigned int sourceLen,int attach_buf );


/* AVPS */

/** 
 * Create and add an AVP to the message, by duplicating the storage space
 */
#define AAACreateAndAddAVPToMessage(_msg_,_code_,_flags_,_vdr_,_data_,_len_) \
	( AAAAddAVPToMessage(_msg_, \
	AAACreateAVP(_code_,_flags_,_vdr_,_data_,_len_, AVP_DUPLICATE_DATA),\
	(_msg_)->avpList.tail) )

AAA_AVP* AAACreateAVP(
			AAA_AVPCode code,
			AAA_AVPFlag flags,
			AAAVendorId vendorId,
			char *data,
			size_t length,
			AVPDataStatus data_status);
typedef AAA_AVP* (*AAACreateAVP_f)(
				AAA_AVPCode code,
				AAA_AVPFlag flags,
				AAAVendorId vendorId,
				char *data,
				size_t length,
				AVPDataStatus data_status);

AAA_AVP* AAACloneAVP(AAA_AVP *avp,unsigned char duplicate_data);

AAAReturnCode AAAAddAVPToMessage(
			AAAMessage *msg,
			AAA_AVP *avp,
			AAA_AVP *position);
typedef AAAReturnCode (*AAAAddAVPToMessage_f)(
			AAAMessage *msg,
			AAA_AVP *avp,
			AAA_AVP *position);


AAA_AVP *AAAFindMatchingAVP(
			AAAMessage *msg,
			AAA_AVP *startAvp,
			AAA_AVPCode avpCode,
			AAAVendorId vendorId,
			AAASearchType searchType);				
typedef AAA_AVP* (*AAAFindMatchingAVP_f)(
			AAAMessage *msg,
			AAA_AVP *startAvp,
			AAA_AVPCode avpCode,
			AAAVendorId vendorId,
			AAASearchType searchType);

AAAReturnCode AAARemoveAVPFromMessage(AAAMessage *msg,AAA_AVP *avp);

AAAReturnCode AAAFreeAVP(AAA_AVP **avp);
typedef AAAReturnCode (*AAAFreeAVP_f)(AAA_AVP **avp);


AAA_AVP* AAAGetFirstAVP(AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetLastAVP(AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetNextAVP(AAA_AVP *avp);
typedef AAA_AVP* (*AAAGetNextAVP_f)(AAA_AVP *avp);


AAA_AVP* AAAGetPrevAVP(AAA_AVP *avp);

char *AAAConvertAVPToString(AAA_AVP *avp,char *dest,unsigned int destLen);

 

str AAAGroupAVPS(AAA_AVP_LIST avps);
typedef str (*AAAGroupAVPS_f)(AAA_AVP_LIST avps);

AAA_AVP_LIST AAAUngroupAVPS(str buf);
typedef AAA_AVP_LIST (*AAAUngroupAVPS_f)(str buf);


AAA_AVP  *AAAFindMatchingAVPList(
			AAA_AVP_LIST avpList,
			AAA_AVP *startAvp,
			AAA_AVPCode avpCode,
			AAAVendorId vendorId,
			AAASearchType searchType);
typedef AAA_AVP  *(*AAAFindMatchingAVPList_f)(
			AAA_AVP_LIST avpList,
			AAA_AVP *startAvp,
			AAA_AVPCode avpCode,
			AAAVendorId vendorId,
			AAASearchType searchType);


void AAAAddAVPToList(AAA_AVP_LIST *list,AAA_AVP *avp);
typedef void (*AAAAddAVPToList_f)(AAA_AVP_LIST *list,AAA_AVP *avp);	


/* CALLBACKS */


int AAAAddRequestHandler(AAARequestHandler_f *f,void *param);
typedef int (*AAAAddRequestHandler_f)(AAARequestHandler_f *f,void *param);

int AAAAddResponseHandler(AAAResponseHandler_f *f,void *param);
typedef int (*AAAAddResponseHandler_f)(AAAResponseHandler_f *f,void *param);

/* MESSAGE SENDING */

AAAReturnCode AAASendMessage(AAAMessage *message,AAATransactionCallback_f *callback_f,void *callback_param);
typedef AAAReturnCode (*AAASendMessage_f)(AAAMessage *message,AAATransactionCallback_f *callback_f,void *callback_param);

AAAReturnCode AAASendMessageToPeer(AAAMessage *message,str *peer_id,AAATransactionCallback_f *callback_f,void *callback_param);
typedef AAAReturnCode (*AAASendMessageToPeer_f)(AAAMessage *message,str *peer_id,AAATransactionCallback_f *callback_f,void *callback_param);

AAAMessage* AAASendRecvMessage(AAAMessage *msg);
typedef AAAMessage* (*AAASendRecvMessage_f)(AAAMessage *msg);

AAAMessage* AAASendRecvMessageToPeer(AAAMessage *msg, str *peer_id);
typedef AAAMessage* (*AAASendRecvMessageToPeer_f)(AAAMessage *msg, str *peer_id);

AAAReturnCode AAAFreeMessage(AAAMessage **message);
typedef AAAReturnCode (*AAAFreeMessage_f)(AAAMessage **message);



#endif /*DIAMETER_API_H_*/
