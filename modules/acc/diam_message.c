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
 * ---------
 *
 *   2003-04-07 created by bogdan
 */

/*! \file
 * \ingroup acc
 * \brief Acc:: Diameter messages
 *
 * - Module: \ref acc
 */

#ifdef DIAM_ACC

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "../../mem/shm_mem.h"
#include "../../dprint.h"
#include "diam_message.h"

#define get_3bytes(_b) \
	((((unsigned int)(_b)[0])<<16)|(((unsigned int)(_b)[1])<<8)|\
	(((unsigned int)(_b)[2])))

#define get_4bytes(_b) \
	((((unsigned int)(_b)[0])<<24)|(((unsigned int)(_b)[1])<<16)|\
	(((unsigned int)(_b)[2])<<8)|(((unsigned int)(_b)[3])))

#define set_3bytes(_b,_v) \
	{(_b)[0]=((_v)&0x00ff0000)>>16;(_b)[1]=((_v)&0x0000ff00)>>8;\
	(_b)[2]=((_v)&0x000000ff);}

#define set_4bytes(_b,_v) \
	{(_b)[0]=((_v)&0xff000000)>>24;(_b)[1]=((_v)&0x00ff0000)>>16;\
	(_b)[2]=((_v)&0x0000ff00)>>8;(_b)[3]=((_v)&0x000000ff);}

#define to_32x_len( _len_ ) \
	( (_len_)+(((_len_)&3)?4-((_len_)&3):0) )


/*! \brief from a AAAMessage structure, a buffer to be send is build
 */
AAAReturnCode AAABuildMsgBuffer( AAAMessage *msg )
{
	char *p;
	AAA_AVP       *avp;

	/* first let's compute the length of the buffer */
	msg->buf.len = AAA_MSG_HDR_SIZE; /* AAA message header size */
	/* count and add the avps */
	for(avp=msg->avpList.head;avp;avp=avp->next) {
		msg->buf.len += AVP_HDR_SIZE(avp->flags)+ to_32x_len( avp->data.len );
	}

	/* allocate some memory */
	msg->buf.s = (char*)ad_malloc( msg->buf.len );
	if (!msg->buf.s) {
		LM_ERR("no more pkg free memory!\n");
		goto error;
	}
	memset(msg->buf.s, 0, msg->buf.len);

	/* fill in the buffer */
	p = msg->buf.s;
	/* DIAMETER HEADER */
	/* message length */
	((unsigned int*)p)[0] =htonl(msg->buf.len);
	/* Diameter Version */
	*p = 1;
	p += VER_SIZE + MESSAGE_LENGTH_SIZE;
	/* command code */
	((unsigned int*)p)[0] = htonl(msg->commandCode);
	/* flags */
	*p = (unsigned char)msg->flags;
	p += FLAGS_SIZE + COMMAND_CODE_SIZE;
	/* application-ID */
	((unsigned int*)p)[0] = htonl(msg->applicationId);
	p += APPLICATION_ID_SIZE;
	/* hop by hop id */
	((unsigned int*)p)[0] = msg->hopbyhopId;
	p += HOP_BY_HOP_IDENTIFIER_SIZE;
	/* end to end id */
	((unsigned int*)p)[0] = msg->endtoendId;
	p += END_TO_END_IDENTIFIER_SIZE;

	/* AVPS */
	for(avp=msg->avpList.head;avp;avp=avp->next) {
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

	if ((char*)p-msg->buf.s!=msg->buf.len) {
		LM_ERR("mismatch between len and buf!\n");
		ad_free( msg->buf.s );
		msg->buf.s = 0;
		msg->buf.len = 0;
		goto error;
	}
	LM_DBG("Message: %.*s\n", msg->buf.len, msg->buf.s);
	return AAA_ERR_SUCCESS;
error:
	return -1;
}



/*! \brief frees a message allocated through AAANewMessage()
 */
AAAReturnCode  AAAFreeMessage(AAAMessage **msg)
{
	AAA_AVP *avp_t;
	AAA_AVP *avp;

	/* param check */
	if (!msg || !(*msg))
		goto done;

	/* free the avp list */
	avp = (*msg)->avpList.head;
	while (avp) {
		avp_t = avp;
		avp = avp->next;
		/*free the avp*/
		AAAFreeAVP(&avp_t);
	}

	/* free the buffer (if any) */
	if ( (*msg)->buf.s )
		ad_free( (*msg)->buf.s );

	/* free the AAA msg */
	ad_free(*msg);
	msg = 0;

done:
	return AAA_ERR_SUCCESS;
}



/*! \brief Sets the proper result_code into the Result-Code AVP; thus avp must already
 * exists into the reply message */
AAAResultCode  AAASetMessageResultCode(
	AAAMessage *message,
	AAAResultCode resultCode)
{
	if ( !is_req(message) && message->res_code) {
		*((unsigned int*)(message->res_code->data.s)) = htonl(resultCode);
		return AAA_ERR_SUCCESS;
	}
	return AAA_ERR_FAILURE;
}



/*! \brief This function convert message to message structure */
AAAMessage* AAATranslateMessage( unsigned char* source, unsigned int sourceLen,
															int attach_buf)
{
	unsigned char *ptr;
	AAAMessage    *msg;
	unsigned char version;
	unsigned int  msg_len;
	AAA_AVP       *avp;
	unsigned int  avp_code;
	unsigned char avp_flags;
	unsigned int  avp_len;
	unsigned int  avp_vendorID;
	unsigned int  avp_data_len;

	/* check the params */
	if( !source || !sourceLen || sourceLen<AAA_MSG_HDR_SIZE) {
		LM_ERR("invalid buffered received!\n");
		goto error;
	}

	/* inits */
	msg = 0;
	avp = 0;
	ptr = source;

	/* alloc a new message structure */
	msg = (AAAMessage*)ad_malloc(sizeof(AAAMessage));
	if (!msg) {
		LM_ERR("no more pkg free memory!!\n");
		goto error;
	}
	memset(msg,0,sizeof(AAAMessage));

	/* get the version */
	version = (unsigned char)*ptr;
	ptr += VER_SIZE;
	if (version!=1) {
		LM_ERR("invalid version [%d]in AAA msg\n",version);
		goto error;
	}

	/* message length */
	msg_len = get_3bytes( ptr );
	ptr += MESSAGE_LENGTH_SIZE;
	if (msg_len>sourceLen) {
		LM_ERR("AAA message len [%d] bigger then buffer len [%d]\n",
				msg_len,sourceLen);
		goto error;
	}

	/* command flags */
	msg->flags = *ptr;
	ptr += FLAGS_SIZE;

	/* command code */
	msg->commandCode = get_3bytes( ptr );
	ptr += COMMAND_CODE_SIZE;

	/* application-Id */
	msg->applicationId = get_4bytes( ptr );
	ptr += APPLICATION_ID_SIZE;

	/* Hop-by-Hop-Id */
	msg->hopbyhopId = *((unsigned int*)ptr);
	ptr += HOP_BY_HOP_IDENTIFIER_SIZE;

	/* End-to-End-Id */
	msg->endtoendId = *((unsigned int*)ptr);
	ptr += END_TO_END_IDENTIFIER_SIZE;

	/* start decoding the AVPS */
	while (ptr < source+msg_len) {
		if (ptr+AVP_HDR_SIZE(0x80)>source+msg_len){
			LM_ERR("source buffer to short!! "
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
			LM_ERR("invalid AVP len [%d]\n", avp_len);
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
		if ( source+msg_len<ptr+avp_data_len) {
			LM_ERR("source buffer to short!! "
				"Cannot read a whole data for AVP!\n");
			goto error;
		}

		/* create the AVP */
		avp = AAACreateAVP( avp_code, avp_flags, avp_vendorID, (char*)ptr,
			avp_data_len, AVP_DONT_FREE_DATA);
		if (!avp)
			goto error;

		/* link the avp into aaa message to the end */
		AAAAddAVPToMessage( msg, avp, msg->avpList.tail);

		ptr += to_32x_len( avp_data_len );
	}

	/* link the buffer to the message */
	if (attach_buf) {
		msg->buf.s = (char*)source;
		msg->buf.len = msg_len;
	}

	//AAAPrintMessage( msg );
	return  msg;
error:
	LM_ERR("message conversion droped!!\n");
	AAAFreeMessage(&msg);
	return 0;
}



/*! \brief print as debug all info contained by an aaa message + AVPs
 */
void AAAPrintMessage( AAAMessage *msg)
{
	char    buf[1024];
	AAA_AVP *avp;

	/* print msg info */
	LM_DBG("AAA_MESSAGE - %p\n",msg);
	LM_DBG("\tCode = %u\n",msg->commandCode);
	LM_DBG("\tFlags = %x\n",msg->flags);

	/*print the AVPs */
	avp = msg->avpList.head;
	while (avp) {
		AAAConvertAVPToString(avp,buf,1024);
		LM_DBG("\n%s\n",buf);
		avp=avp->next;
	}
}

AAAMessage* AAAInMessage(AAACommandCode cmdCode, 
				AAAApplicationId appID)
{
	AAAMessage *msg;

	/* allocated a new AAAMessage structure a set it to 0 */
	msg = (AAAMessage*)ad_malloc(sizeof(AAAMessage));
	if (!msg) {
		LM_ERR("no more pkg free memory!\n");
		return NULL;
	}
	memset(msg, 0, sizeof(AAAMessage));

	/* command code */
	msg->commandCode = cmdCode;

	/* application ID */
	msg->applicationId = appID;

	/* it's a new request -> set the flag */
	msg->flags = 0x80;

	return msg;
}

#endif
