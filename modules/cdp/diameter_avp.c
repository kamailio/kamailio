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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "diameter.h"
#include "utils.h"


/* Start of disc implementation */

/**
 * Takes care that each AVP type has the default flags set/reset and a proper data type.
 * All this default values (for flags and data-type) are correct/set by this
 * function.
 * @param code - code of the AVP
 * @param avp - the actual AVP to set flags
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
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



/** 
 * This function creates an AVP and returns a pointer to it.
 * @param code - the code of the new AVP
 * @param flags - the flags to set
 * @param vendorId - vendor id
 * @param data - the generic payload data
 * @param length - length of the payload
 * @param data_status - what to do with the payload: duplicate, free with the message, etc
 * @returns the AAA_AVP* or null on error
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
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
		LM_ERR("AAACreateAVP: NULL value received for"
			" param data/length (AVP Code %d, VendorId %d)!!\n",code,vendorId);
		return 0;
	}

	/* allocated a new AVP struct */
	avp = 0;
	avp = (AAA_AVP*)shm_malloc(sizeof(AAA_AVP));
	if (!avp)
		goto error;
	memset( avp, 0, sizeof(AAA_AVP) );

	/* set some fields */
	//avp->free_it = free_it;
	avp->code=code;
	avp->flags=flags;
	avp->vendorId=vendorId;
	set_avp_fields( code, avp);

	if ( data_status==AVP_DUPLICATE_DATA ) {
		/* make a duplicate for data */
		avp->data.len = length;
		avp->data.s = (void*)shm_malloc(length);
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
	LM_ERR("AAACreateAVP: no more free memory!\n");
	return 0;
}



/**
 *  Insert the AVP avp into the avpList of a message, after a certain position.
 * @param msg - the AAAMessage to add to
 * @param avp - the AAA_AVP to add
 * @param position - AAA_AVP to add after. If NULL, will add at the beginning.
 * @returns AAA_ERR_SUCCESS on success or AAA_ERR_PARAMETER if the position is not found
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAAReturnCode  AAAAddAVPToMessage(
	AAAMessage *msg,
	AAA_AVP *avp,
	AAA_AVP *position)
{
	AAA_AVP *avp_t;

	if ( !msg || !avp ) {
		LM_ERR("AAAAddAVPToMessage: param msg or avp passed null"
			" or *avpList=NULL and position!=NULL !!\n");
		return AAA_ERR_PARAMETER;
	}

	if (!position) {
		/* insert at the begining */
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
			LM_ERR("AAAAddAVPToMessage: the \"position\" avp is not in"
				"\"msg\" message!!\n");
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



/**
 *  This function finds an AVP with matching code and vendor id.
 * @param msg - message to look into
 * @param startAvp - where to start the search. Usefull when you want to find the next one.
 * 	Even this one will be checked and can be returned if it fits.
 * @param avpCode - code of the AVP to match
 * @param vendorId - vendor id to match
 * @param searchType - whether to look forward or backward
 * @returns the AAA_AVP* if found, NULL if not 
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
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
		LM_ERR("FindMatchingAVP: param msg passed null !!\n");
		goto error;
	}

	/* where should I start searching from ? */
	if (startAvp) {
		/* double-check the startAVP avp */
		for(avp_t=msg->avpList.head;avp_t&&avp_t!=startAvp;avp_t=avp_t->next);
		if (!avp_t) {
			LM_ERR("AAAFindMatchingAVP: the \"position\" avp is not "
				"in \"avpList\" list!!\n");
			goto error;
		}
		avp_t=startAvp;
	} else {
		/* if no startAVP -> start from one of the ends */
		avp_t=(searchType==AAA_FORWARD_SEARCH)?(msg->avpList.head):
			(msg->avpList.tail);
	}

	/* start searching */
	while(avp_t) {
		if (avp_t->code==avpCode && avp_t->vendorId==vendorId)
			return avp_t;
		avp_t = (searchType==AAA_FORWARD_SEARCH)?(avp_t->next):(avp_t->prev);
	}

error:
	return 0;
}



/**
 *  This function removes an AVP from a message.
 * @param msg - the diameter message 
 * @param avp - the AVP to remove
 * @returns AAA_ERR_SUCCESS on success or AAA_ERR_PARAMETER if not found
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAAReturnCode  AAARemoveAVPFromMessage(
	AAAMessage *msg,
	AAA_AVP *avp)
{
	AAA_AVP *avp_t;

	/* param check */
	if ( !msg || !avp ) {
		LM_ERR("AAARemoveAVPFromMessage: param AVP_LIST \"avpList\" or AVP "
			"\"avp\" passed null !!\n");
		return AAA_ERR_PARAMETER;
	}

	/* search the "avp" avp */
	for(avp_t=msg->avpList.head;avp_t&&avp_t!=avp;avp_t=avp_t->next);
	if (!avp_t) {
		LM_ERR("AAARemoveAVPFromMessage: the \"avp\" avp is not in "
			"\"avpList\" avp list!!\n");
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



/**
 *  The function frees the memory allocated to an AVP 
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAAReturnCode  AAAFreeAVP(AAA_AVP **avp)
{
	/* some checks */
	if (!avp || !(*avp)) {
		LM_ERR("AAAFreeAVP: param avp cannot be null!!\n");
		return AAA_ERR_PARAMETER;
	}

	/* free all the mem */
	if ( (*avp)->free_it && (*avp)->data.s )
		shm_free((*avp)->data.s);

	shm_free( *avp );
	avp = 0;

	return AAA_ERR_SUCCESS;
}



/**
 *  This function returns the first AVP in the list.
 * @param avpList - the list 
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAA_AVP*  AAAGetFirstAVP(AAA_AVP_LIST *avpList){
	return avpList->head;
}



/**
 *  This function returns the last AVP in the list.
 * @param avpList - the list  
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAA_AVP*  AAAGetLastAVP(AAA_AVP_LIST *avpList)
{
	return avpList->tail;
}




/**
 *  This function returns the next AVP in the list that this AVP was extracted from
 * @param avp - reference avp
 * @returns  the next AAA_AVP or NULL if this was the last one
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAA_AVP*  AAAGetNextAVP(AAA_AVP *avp)
{
	return avp->next;
}



/**
 *  This function returns a the previous AVP in the list  that this AVP was extracted from
 * @param avp - reference avp
 * @returns  the next AAA_AVP or NULL if this was the first one
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAA_AVP*  AAAGetPrevAVP(AAA_AVP *avp)
{
	return avp->prev;
}



/**
 *  This function converts the data in the AVP to a format suitable for
 * print, log or display.
 * @param avp - the AAA_AVP to print
 * @param dest - preallocated destination buffer. If too short, message will be truncated 
 * @param destLen - length of the destipation buffer 
 * @returns dest on success, NULL on failure 
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
char*  AAAConvertAVPToString(AAA_AVP *avp, char *dest, unsigned int destLen)
{
	int l;
	int i;

	if (!avp || !dest || !destLen) {
		LM_ERR("AAAConvertAVPToString: param AVP, DEST or DESTLEN "
			"passed as null!!!\n");
		return 0;
	}
	l = snprintf(dest,destLen,"AVP(%p < %p >%p);code=%u,"
		"flags=%x;\nDataType=%u;VendorID=%u;DataLen=%u;\n",
		avp->prev,avp,avp->next,avp->code,avp->flags,
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
			LM_WARN("AAAConvertAVPToString: don't know how to print"
				" this data type [%d] -> tryng hexa\n",avp->type);
		case AAA_AVP_DATA_TYPE:
			for (i=0;i<avp->data.len&&l<destLen-1;i++)
			l+=snprintf(dest+l,destLen-l-1,"%x",
				((unsigned char*)avp->data.s)[i]);
	}
	return dest;
}


/**
 * Duplicate a whole AAA_AVP.
 * @param avp - original avp
 * @param clone_data - whether to duplicate also the data payload
 * @returns the new AAA_AVP* or NULL on error
 * \note This function is taken from DISC http://developer.berlios.de/projects/disc/
 */
AAA_AVP* AAACloneAVP( AAA_AVP *avp , unsigned char clone_data)
{
	AAA_AVP *n_avp;

	if (!avp || !(avp->data.s) || !(avp->data.len) )
		goto error;

	/* clone the avp structure */
	n_avp = (AAA_AVP*)shm_malloc( sizeof(AAA_AVP) );
	if (!n_avp) {
		LM_ERR("clone_avp: cannot get free memory!!\n");
		goto error;
	}
	memcpy( n_avp, avp, sizeof(AAA_AVP));
	n_avp->next = n_avp->prev = 0;

	if (clone_data) {
		/* clone the avp data */
		n_avp->data.s = (char*)shm_malloc( avp->data.len );
		if (!(n_avp->data.s)) {
			LM_ERR("clone_avp: cannot get free memory!!\n");
			shm_free( n_avp );
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

/* End of disc implementation */

/* Simple extension for grouped avp based on the above implementation  */

/**
 * Adds an AVP to a list of AVPs, at the end.
 * @param list - the list to add to
 * @param avp - the avp to add 
 */ 
void AAAAddAVPToList(AAA_AVP_LIST *list,AAA_AVP *avp)
{
	if (list->tail) {
		avp->prev=list->tail;
		avp->next=0;	
		list->tail->next = avp;
		list->tail=avp;
	} else {
		list->head = avp;
		list->tail = avp;
		avp->next=0;
		avp->prev=0;
	}	
}
 
/** 
 * Groups a list of avps into a data buffer
 * @param avps 
 */
str AAAGroupAVPS(AAA_AVP_LIST avps)
 {
 	AAA_AVP *avp;
	unsigned char *p;
	str buf={0,0};

	/* count and add the avps */
	for(avp=avps.head;avp;avp=avp->next) {
		buf.len += AVP_HDR_SIZE(avp->flags)+ to_32x_len( avp->data.len );
	}

	if (!buf.len) return buf;
	/* allocate some memory */
	buf.s = (char*)shm_malloc( buf.len );
	if (!buf.s) {
		LM_ERR("hss3g_group_avps: no more free memory!\n");
		buf.len=0;
		return buf;
	}
	memset(buf.s, 0, buf.len);
	/* fill in the buffer */
	p = (unsigned char*) buf.s;
	for(avp=avps.head;avp;avp=avp->next) {
		/* AVP HEADER */
		/* avp code */
		set_4bytes(p,avp->code);
		p +=4;
		/* flags */
		(*p++) = (unsigned char)avp->flags;
		/* avp length */
		set_3bytes(p, (AVP_HDR_SIZE(avp->flags)+avp->data.len) );
		p += 3;
		/* vendor id */
		if ((avp->flags&0x80)!=0) {
			set_4bytes(p,avp->vendorId);
			p +=4;
		}
		/* data */
		memcpy( p, avp->data.s, avp->data.len);
		p += to_32x_len( avp->data.len );
	}
	if ((char*)p-buf.s!=buf.len) {
		LM_ERR("BUG:hss3g_group_avps: mismatch between len and buf!\n");
		shm_free( buf.s );
		buf.s = 0;
		buf.len = 0;
		return buf;
	}
	return buf;
}

/** 
 * Ungroup from a data buffer a list of avps
 * @param buf - payload to ungroup the list from
 * @returns the AAA_AVP_LIST or an empty one on error 
 */
AAA_AVP_LIST AAAUngroupAVPS(str buf)
{
	char *ptr;
	AAA_AVP       *avp;
	unsigned int  avp_code;
	unsigned char avp_flags;
	unsigned int  avp_len;
	unsigned int  avp_vendorID;
	unsigned int  avp_data_len;
	AAA_AVP_LIST	lh;

	lh.head=0;
	lh.tail=0;
	ptr = buf.s;

	/* start decoding the AVPS */
	while (ptr < buf.s+buf.len) {
		if (ptr+AVP_HDR_SIZE(0x80)>buf.s+buf.len){
			LM_ERR("hss3g_ungroup_avps: source buffer to short!! "
				"Cannot read the whole AVP header!\n");
			goto error;
		}
		/* avp code */
		avp_code = get_4bytes( ptr );
		ptr += AVP_CODE_SIZE;
		/* avp flags */
		avp_flags = (unsigned char)*ptr;
		ptr += AVP_FLAGS_SIZE;
		/* avp length */
		avp_len = get_3bytes( ptr );
		ptr += AVP_LENGTH_SIZE;
		if (avp_len<1) {
			LM_ERR("hss3g_ungroup_avps: invalid AVP len [%d]\n",
				avp_len);
			goto error;
		}
		/* avp vendor-ID */
		avp_vendorID = 0;
		if (avp_flags&AAA_AVP_FLAG_VENDOR_SPECIFIC) {
			avp_vendorID = get_4bytes( ptr );
			ptr += AVP_VENDOR_ID_SIZE;
		}

		/* data length */
		avp_data_len = avp_len-AVP_HDR_SIZE(avp_flags);
		/*check the data length */
		if ( buf.s+buf.len<ptr+avp_data_len) {
			LM_ERR("hss3g_ungroup_avps: source buffer to short!! "
				"Cannot read a whole data for AVP!\n");
			goto error;
		}

		/* create the AVP */
		avp = AAACreateAVP( avp_code, avp_flags, avp_vendorID, ptr,
			avp_data_len, AVP_DONT_FREE_DATA);
		if (!avp) {
			LM_ERR("hss3g_ungroup_avps: can't create avp for member of list\n");
			goto error;
		}

		/* link the avp into aaa message to the end */
  		avp->next = 0;
		avp->prev = lh.tail;
		if (lh.tail) {
			lh.tail->next=avp;
			lh.tail=avp;
		}
		else {
			lh.tail=avp;
			lh.head=avp;
		}

		ptr += to_32x_len( avp_data_len );
	}
	return lh;

error:
	LM_CRIT("AVP:<%.*s>\n",buf.len,buf.s);
	return lh;
}

/**
 * Find an avp into a list of avps.
 * @param avpList - the list to look into
 * @param startAvp - where to start the search. Usefull when you want to find the next one. 
 * Even this one will be checked and can be returned if it fits.
 * @param avpCode - the AVP code to match
 * @param vendorId - the vendor id to match
 * @param searchType - direction of search
 * @returns the AAA_AVP* if found, NULL if not
 */
AAA_AVP  *AAAFindMatchingAVPList(
	AAA_AVP_LIST avpList,
	AAA_AVP *startAvp,
	AAA_AVPCode avpCode,
	AAAVendorId vendorId,
	AAASearchType searchType)
{
	AAA_AVP *avp_t;

	/* param checking */

	/* where should I start searching from ? */
	if (startAvp) {
		/* double-check the startAVP avp */
		for(avp_t=avpList.head;avp_t&&avp_t!=startAvp;avp_t=avp_t->next);
		if (!avp_t) {
			LM_ERR("ndMatchingAVP: the \"position\" avp is not "
				"in \"avpList\" list!!\n");
			goto error;
		}
		avp_t=startAvp;
	} else {
		/* if no startAVP -> start from one of the ends */
		avp_t=(searchType==AAA_FORWARD_SEARCH)?(avpList.head):
			(avpList.tail);
	}

	/* start searching */
	while(avp_t) {
		if (avp_t->code==avpCode && avp_t->vendorId==vendorId)
			return avp_t;
		avp_t = (searchType==AAA_FORWARD_SEARCH)?(avp_t->next):(avp_t->prev);
	}

error:
	return 0;
}
 
