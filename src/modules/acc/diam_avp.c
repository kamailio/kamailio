/*
 * $Id$
 *
 * Copyright (C) 2002-2003 FhG Fokus
 *
 * This file is part of disc, a free diameter server/client.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2002-10-04 created  by illya (komarov@fokus.gmd.de)
 * 2003-03-12 converted to shm_malloc/free (andrei)
 *
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Diameter support
 *
 * - Module: \ref acc
 */

#ifdef DIAM_ACC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "diam_message.h"



/*
 * each AVP type has some default set/reset flags and a proper data type.
 * All this default values (for flags and data-type) are correct/set by this
 * function.
 */
inline void set_avp_fields( AAA_AVPCode code, AAA_AVP *avp)
{
	switch (code) {
		case   1: /*AVP_User_Name*/
		case  25: /*AVP_Class*/
		case 263: /*AVP_Session_Id*/
		case 283: /*AVP_Destination_Realm*/
		case 293: /*AVP Destination Host*/
		case 264: /*AVP_Origin_Host*/
		case 296: /*AVP Origin_Realm*/
		case 400: /* AVP_Resource */	
		case 401: /* AVP_Response */	
		case 402: /* AVP_Chalenge */	
		case 403: /* AVP_Method */
		case 404: /* Service_Type AVP */
		case 405: /* User_Group AVP*/
		case 279:					
			avp->flags = 0x40|(0x20&avp->flags);
			avp->type = AAA_AVP_STRING_TYPE;
			break;
		case  27: /*AVP_Session_Timeout*/
		case 258: /*AVP_Auth_Aplication_Id*/
		case 262: /*AVP_Redirect_Max_Cache_Time*/
		case 265: /*AVP_Supported_Vendor_Id*/
		case 266: /*AVP_Vendor_Id*/
		case 268: /*AVP_Result_Code*/
		case 270: /*AVP_Session_Binding*/
		case 276: /*AVP_Auth_Grace_Period*/
		case 278: /*AVP_Origin_State_Id*/
		case 291: /*AVP_Authorization_Lifetime*/
			avp->flags = 0x40|(0x20&avp->flags);
			avp->type = AAA_AVP_INTEGER32_TYPE;
			break;
		case 33: /*AVP_Proxy_State*/
			avp->flags = 0x40;
			avp->type = AAA_AVP_STRING_TYPE;
			break;
		case 257: /*AVP_Host_IP_Address*/
			avp->flags = 0x40|(0x20&avp->flags);
			avp->type = AAA_AVP_ADDRESS_TYPE;
			break;
		case 269: /*AVP_Product_Name*/
			avp->flags = 0x00;
			avp->type = AAA_AVP_STRING_TYPE;
			break;
		case 281: /*AVP_Error_Message*/
			avp->flags = (0x20&avp->flags);
			avp->type = AAA_AVP_STRING_TYPE;
			break;
		default:
			avp->type = AAA_AVP_DATA_TYPE;
	};
}



/* This function creates an AVP and returns a pointer to it;
 */
AAA_AVP*  AAACreateAVP(
	AAA_AVPCode code,
	AAA_AVPFlag flags,
	AAAVendorId vendorId,
	char   *data,
	size_t length,
	AVPDataStatus data_status)
{
	AAA_AVP *avp;

	/* first check the params */
	if( data==0 || length==0) {
		LM_ERR("null value received for param data/length !!\n");
		return 0;
	}

	/* allocated a new AVP struct */
	avp = 0;
	avp = (AAA_AVP*)ad_malloc(sizeof(AAA_AVP));
	if (!avp)
		goto error;
	memset( avp, 0, sizeof(AAA_AVP) );

	/* set some fields */
	//avp->free_it = free_it;
	avp->packetType = AAA_DIAMETER;
	avp->code=code;
	avp->flags=flags;
	avp->vendorId=vendorId;
	set_avp_fields( code, avp);

	if ( data_status==AVP_DUPLICATE_DATA ) {
		/* make a duplicate for data */
		avp->data.len = length;
		avp->data.s = (void*)ad_malloc(length);
		if(!avp->data.s)
			goto error;
		memcpy( avp->data.s, data, length);
		avp->free_it = 1;
	} else {
		avp->data.s = data;
		avp->data.len = length;
		avp->free_it = (data_status==AVP_FREE_DATA)?1:0;
	}

	return avp;
error:
	LM_ERR("no more free memoryfor a new AVP!\n");
	return 0;
}



/* Insert the AVP avp into this avpList of a message after position */
AAAReturnCode  AAAAddAVPToMessage(
	AAAMessage *msg,
	AAA_AVP *avp,
	AAA_AVP *position)
{
	AAA_AVP *avp_t;

	if ( !msg || !avp ) {
		LM_ERR("param msg or avp passed null or *avpList=NULL "
				"and position!=NULL !!\n");
		return AAA_ERR_PARAMETER;
	}

	if (!position) {
		/* insert at the beginning */
		avp->next = msg->avpList.head;
		avp->prev = 0;
		msg->avpList.head = avp;
		if (avp->next)
			avp->next->prev = avp;
		else
			msg->avpList.tail = avp;
	} else {
		/* look after avp from position */
		for(avp_t=msg->avpList.head;avp_t&&avp_t!=position;avp_t=avp_t->next);
		if (!avp_t) {
			LM_ERR("the \"position\" avp is not in \"msg\" message!!\n");
			return AAA_ERR_PARAMETER;
		}
		/* insert after position */
		avp->next = position->next;
		position->next = avp;
		if (avp->next)
			avp->next->prev = avp;
		else
			msg->avpList.tail = avp;
		avp->prev = position;
	}

	/* update the short-cuts */
	switch (avp->code) {
		case AVP_Session_Id: msg->sessionId = avp;break;
		case AVP_Origin_Host: msg->orig_host = avp;break;
		case AVP_Origin_Realm: msg->orig_realm = avp;break;
		case AVP_Destination_Host: msg->dest_host = avp;break;
		case AVP_Destination_Realm: msg->dest_realm = avp;break;
		case AVP_Result_Code: msg->res_code = avp;break;
		case AVP_Auth_Session_State: msg->auth_ses_state = avp;break;
	}

	return AAA_ERR_SUCCESS;
}


/* This function finds an AVP with matching code and vendor id */
AAA_AVP  *AAAFindMatchingAVP(
	AAAMessage *msg,
	AAA_AVP *startAvp,
	AAA_AVPCode avpCode,
	AAAVendorId vendorId,
	AAASearchType searchType)
{
	AAA_AVP *avp_t;

	/* param checking */
	if (!msg) {
		LM_ERR("param msg passed null !!\n");
		goto error;
	}
	/* search the startAVP avp */
	for(avp_t=msg->avpList.head;avp_t&&avp_t!=startAvp;avp_t=avp_t->next);
	if (!avp_t && startAvp) {
		LM_ERR("the \"position\" avp is not in \"avpList\" list!!\n");
		goto error;
	}

	/* where should I start searching from ? */
	if (!startAvp)
		avp_t=(searchType==AAA_FORWARD_SEARCH)?(msg->avpList.head):
			(msg->avpList.tail);
	else
		avp_t=startAvp;

	/* start searching */
	while(avp_t) {
		if (avp_t->code==avpCode && avp_t->vendorId==vendorId)
			return avp_t;
		avp_t = (searchType==AAA_FORWARD_SEARCH)?(avp_t->next):(avp_t->prev);
	}

error:
	return 0;
}




/* This function removes an AVP from a list of a message */
AAAReturnCode  AAARemoveAVPFromMessage(
	AAAMessage *msg,
	AAA_AVP *avp)
{
	AAA_AVP *avp_t;

	/* param check */
	if ( !msg || !avp ) {
		LM_ERR("param AVP_LIST \"avpList\" or AVP \"avp\" passed null !!\n");
		return AAA_ERR_PARAMETER;
	}

	/* search the "avp" avp */
	for(avp_t=msg->avpList.head;avp_t&&avp_t!=avp;avp_t=avp_t->next);
	if (!avp_t) {
		LM_ERR("the \"avp\" avp is not in \"avpList\" avp list!!\n");
		return AAA_ERR_PARAMETER;
	}

	/* remove the avp from list */
	if (msg->avpList.head==avp)
		msg->avpList.head = avp->next;
	else
		avp->prev->next = avp->next;
	if (avp->next)
		avp->next->prev = avp->prev;
	else
		msg->avpList.tail = avp->prev;
	avp->next = avp->prev = 0;

	/* update short-cuts */
	switch (avp->code) {
		case AVP_Session_Id: msg->sessionId = 0;break;
		case AVP_Origin_Host: msg->orig_host = 0;break;
		case AVP_Origin_Realm: msg->orig_realm = 0;break;
		case AVP_Destination_Host: msg->dest_host = 0;break;
		case AVP_Destination_Realm: msg->dest_realm = 0;break;
		case AVP_Result_Code: msg->res_code = 0;break;
		case AVP_Auth_Session_State: msg->auth_ses_state = 0;break;
	}

	return AAA_ERR_SUCCESS;
}



/* The function frees an AVP */
AAAReturnCode  AAAFreeAVP(AAA_AVP **avp)
{
	/* some checks */
	if (!avp || !(*avp)) {
		LM_ERR("param avp cannot be null!!\n");
		return AAA_ERR_PARAMETER;
	}

	/* free all the mem */
	if ( (*avp)->free_it && (*avp)->data.s )
		ad_free((*avp)->data.s);

	ad_free( *avp );
	*avp = 0;

	return AAA_ERR_SUCCESS;
}



/* This function returns a pointer to the first AVP in the list */
AAA_AVP*  AAAGetFirstAVP(AAA_AVP_LIST *avpList){
	return avpList->head;
}



/* This function returns a pointer to the last AVP in the list */
AAA_AVP*  AAAGetLastAVP(AAA_AVP_LIST *avpList)
{
	return avpList->tail;
}




/* This function returns a pointer to the next AVP in the list */
AAA_AVP*  AAAGetNextAVP(AAA_AVP *avp)
{
	return avp->next;
}



/* This function returns a pointer to the previous AVP in the list */
AAA_AVP*  AAAGetPrevAVP(AAA_AVP *avp)
{
	return avp->prev;
}



/* This function converts the data in the AVP to a format suitable for
 * log or display functions. */
char*  AAAConvertAVPToString(AAA_AVP *avp, char *dest, unsigned int destLen)
{
	int l;
	int i;

	if (!avp || !dest || !destLen) {
		LM_ERR("param AVP, DEST or DESTLEN passed as null!!!\n");
		return 0;
	}
	l = snprintf(dest,destLen,"AVP(%p < %p >%p):packetType=%u;code=%u,"
		"flags=%x;\nDataType=%u;VendorID=%u;DataLen=%u;\n",
		avp->prev,avp,avp->next,avp->packetType,avp->code,avp->flags,
		avp->type,avp->vendorId,avp->data.len);
	switch(avp->type) {
		case AAA_AVP_STRING_TYPE:
			l+=snprintf(dest+l,destLen-l,"String: <%.*s>",avp->data.len,
				avp->data.s);
			break;
		case AAA_AVP_INTEGER32_TYPE:
			l+=snprintf(dest+l,destLen-l,"Int32: <%u>(%x)",
				htonl(*((unsigned int*)avp->data.s)),
				htonl(*((unsigned int*)avp->data.s)));
			break;
		case AAA_AVP_ADDRESS_TYPE:
			i = 1;
			switch (avp->data.len) {
				case 4: i=i*0;
				case 6: i=i*2;
					l+=snprintf(dest+l,destLen-l,"Address IPv4: <%d.%d.%d.%d>",
						(unsigned char)avp->data.s[i+0],
						(unsigned char)avp->data.s[i+1],
						(unsigned char)avp->data.s[i+2],
						(unsigned char)avp->data.s[i+3]);
					break;
				case 16: i=i*0;
				case 18: i=i*2;
					l+=snprintf(dest+l,destLen-l,
						"Address IPv6: <%x.%x.%x.%x.%x.%x.%x.%x>",
						((avp->data.s[i+0]<<8)+avp->data.s[i+1]),
						((avp->data.s[i+2]<<8)+avp->data.s[i+3]),
						((avp->data.s[i+4]<<8)+avp->data.s[i+5]),
						((avp->data.s[i+6]<<8)+avp->data.s[i+7]),
						((avp->data.s[i+8]<<8)+avp->data.s[i+9]),
						((avp->data.s[i+10]<<8)+avp->data.s[i+11]),
						((avp->data.s[i+12]<<8)+avp->data.s[i+13]),
						((avp->data.s[i+14]<<8)+avp->data.s[i+15]));
					break;
			break;
			}
			break;
		//case AAA_AVP_INTEGER64_TYPE:
		case AAA_AVP_TIME_TYPE:
		default:
			LM_WARN("don't know how to print"
				" this data type [%d] -> trying hexa\n",avp->type);
		case AAA_AVP_DATA_TYPE:
			for (i=0;i<avp->data.len&&l<destLen-1;i++)
			l+=snprintf(dest+l,destLen-l-1,"%x",
				((unsigned char*)avp->data.s)[i]);
	}
	return dest;
}



AAA_AVP* AAACloneAVP( AAA_AVP *avp , unsigned char clone_data)
{
	AAA_AVP *n_avp;

	if (!avp || !(avp->data.s) || !(avp->data.len) )
		goto error;

	/* clone the avp structure */
	n_avp = (AAA_AVP*)ad_malloc( sizeof(AAA_AVP) );
	if (!n_avp) {
		LM_ERR("cannot get free memory!!\n");
		goto error;
	}
	memcpy( n_avp, avp, sizeof(AAA_AVP));
	n_avp->next = n_avp->prev = 0;

	if (clone_data) {
		/* clone the avp data */
		n_avp->data.s = (char*)ad_malloc( avp->data.len );
		if (!(n_avp->data.s)) {
			LM_ERR("cannot get free memory!!\n");
			ad_free( n_avp );
			goto error;
		}
		memcpy( n_avp->data.s, avp->data.s, avp->data.len);
		n_avp->free_it = 1;
	} else {
		/* link the clone's data to the original's data */
		n_avp->data.s = avp->data.s;
		n_avp->data.len = avp->data.len;
		n_avp->free_it = 0;
	}

	return n_avp;
error:
	return 0;
}

#endif
