/*
 * $Id$
 *
 * 2003-04-07 created by bogdan
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
 */



#ifndef _AAA_DIAMETER_MSG_H
#define _AAA_DIAMETER_MSG_H

#include "../../str.h"
#include "../../mem/mem.h"

#define ad_malloc	pkg_malloc
#define ad_free		pkg_free

/*********************************** AAA TYPES *******************************/

#define AAA_NO_VENDOR_ID           0

#define VER_SIZE                   1
#define MESSAGE_LENGTH_SIZE        3
#define FLAGS_SIZE                 1
#define COMMAND_CODE_SIZE          3
#define APPLICATION_ID_SIZE        4
#define HOP_BY_HOP_IDENTIFIER_SIZE 4
#define END_TO_END_IDENTIFIER_SIZE 4
#define AVP_CODE_SIZE      4
#define AVP_FLAGS_SIZE     1
#define AVP_LENGTH_SIZE    3
#define AVP_VENDOR_ID_SIZE 4

#define AAA_MSG_HDR_SIZE  \
	(VER_SIZE + MESSAGE_LENGTH_SIZE + FLAGS_SIZE + COMMAND_CODE_SIZE +\
	APPLICATION_ID_SIZE+HOP_BY_HOP_IDENTIFIER_SIZE+END_TO_END_IDENTIFIER_SIZE)

#define AVP_HDR_SIZE(_flags_)  \
	(AVP_CODE_SIZE+AVP_FLAGS_SIZE+AVP_LENGTH_SIZE+\
	AVP_VENDOR_ID_SIZE*(((_flags_)&AAA_AVP_FLAG_VENDOR_SPECIFIC)!=0) )

/* message codes
 */
#ifndef WORDS_BIGENDIAN
	#define AS_MSG_CODE      0x12010000
	#define AC_MSG_CODE      0x0f010000
	#define CE_MSG_CODE      0x01010000
	#define DW_MSG_CODE      0x18010000
	#define DP_MSG_CODE      0x1a010000
	#define RA_MSG_CODE      0x02010000
	#define ST_MSG_CODE      0x13010000
	#define MASK_MSG_CODE    0xffffff00
#else
	#error BIG endian detected!!
	#define AS_MSG_CODE      0x00000112
	#define AC_MSG_CODE      0x0000010f
	#define CE_MSG_CODE      0x00000101
	#define DW_MSG_CODE      0x00000118
	#define DP_MSG_CODE      0x0000011a
	#define RA_MSG_CODE      0x00000102
	#define ST_MSG_CODE      0x00000113
	#define MASK_MSG_CODE    0x00ffffff
#endif



typedef unsigned int    AAACommandCode;
typedef unsigned int    AAAVendorId;
typedef unsigned int    AAAExtensionId;
typedef unsigned int    AAA_AVPCode;
typedef unsigned int    AAAValue;
typedef unsigned int    AAAApplicationId;
typedef void*           AAAApplicationRef;
typedef str             AAASessionId;
typedef unsigned int    AAAMsgIdentifier;
typedef unsigned char   AAAMsgFlag;



/* Status codes returned by functions in the AAA API */
typedef enum {
	AAA_ERR_NOT_FOUND = -2,         /* handle or id not found */
	AAA_ERR_FAILURE   = -1,         /* unspecified failure during an AAA op. */
	AAA_ERR_SUCCESS   =  0,         /* AAA operation succeeded */
	AAA_ERR_NOMEM,                  /* op. caused memory to be exhausted */
	AAA_ERR_PROTO,                  /*  AAA protocol error */
	AAA_ERR_SECURITY,
	AAA_ERR_PARAMETER,
	AAA_ERR_CONFIG,
	AAA_ERR_UNKNOWN_CMD,
	AAA_ERR_MISSING_AVP,
	AAA_ERR_ALREADY_INIT,
	AAA_ERR_TIMED_OUT,
	AAA_ERR_CANNOT_SEND_MSG,
	AAA_ERR_ALREADY_REGISTERED,
	AAA_ERR_CANNOT_REGISTER,
	AAA_ERR_NOT_INITIALIZED,
	AAA_ERR_NETWORK_ERROR,
} AAAReturnCode;


/* The following are AVP data type codes. They correspond directly to
 * the AVP data types outline in the Diameter specification [1]: */
typedef enum {
	AAA_AVP_DATA_TYPE,
	AAA_AVP_STRING_TYPE,
	AAA_AVP_ADDRESS_TYPE,
	AAA_AVP_INTEGER32_TYPE,
	AAA_AVP_INTEGER64_TYPE,
	AAA_AVP_TIME_TYPE,
} AAA_AVPDataType;


/* The following are used for AVP header flags and for flags in the AVP
 *  wrapper struct and AVP dictionary definitions. */
typedef enum {
	AAA_AVP_FLAG_NONE               = 0x00,
	AAA_AVP_FLAG_MANDATORY          = 0x40,
	AAA_AVP_FLAG_RESERVED           = 0x1F,
	AAA_AVP_FLAG_VENDOR_SPECIFIC    = 0x80,
	AAA_AVP_FLAG_END_TO_END_ENCRYPT = 0x20,
} AAA_AVPFlag;


/* List with all known application identifiers */
typedef enum {
	AAA_APP_DIAMETER_COMMON_MSG  = 0,
	AAA_APP_NASREQ               = 1,
	AAA_APP_MOBILE_IP            = 2,
	AAA_APP_DIAMETER_BASE_ACC    = 3,
	AAA_APP_RELAY                = (int)0xffffffff,
}AAA_APP_IDS;


/* The following are the result codes returned from remote servers as
 * part of messages */
typedef enum {
	AAA_MUTI_ROUND_AUTH           = 1001,
	AAA_SUCCESS                   = 2001,
	AAA_COMMAND_UNSUPPORTED       = 3001,
	AAA_UNABLE_TO_DELIVER         = 3002,
	AAA_REALM_NOT_SERVED          = 3003,
	AAA_TOO_BUSY                  = 3004,
	AAA_LOOP_DETECTED             = 3005,
	AAA_REDIRECT_INDICATION       = 3006,
	AAA_APPLICATION_UNSUPPORTED   = 3007,
	AAA_INVALID_HDR_BITS          = 3008,
	AAA_INVALID_AVP_BITS          = 3009,
	AAA_UNKNOWN_PEER              = 3010,
	AAA_AUTHENTICATION_REJECTED   = 4001,
	AAA_OUT_OF_SPACE              = 4002,
	AAA_ELECTION_LOST             = 4003,
	AAA_AVP_UNSUPPORTED           = 5001,
	AAA_UNKNOWN_SESSION_ID        = 5002,
	AAA_AUTHORIZATION_REJECTED    = 5003,
	AAA_INVALID_AVP_VALUE         = 5004,
	AAA_MISSING_AVP               = 5005,
	AAA_RESOURCES_EXCEEDED        = 5006,
	AAA_CONTRADICTING_AVPS        = 5007,
	AAA_AVP_NOT_ALLOWED           = 5008,
	AAA_AVP_OCCURS_TOO_MANY_TIMES = 5009,
	AAA_NO_COMMON_APPLICATION     = 5010,
	AAA_UNSUPPORTED_VERSION       = 5011,
	AAA_UNABLE_TO_COMPLY          = 5012,
	AAA_INVALID_BIT_IN_HEADER     = 5013,
	AAA_INVALIS_AVP_LENGTH        = 5014,
	AAA_INVALID_MESSGE_LENGTH     = 5015,
	AAA_INVALID_AVP_BIT_COMBO     = 5016,
	AAA_NO_COMMON_SECURITY        = 5017,
} AAAResultCode;


typedef enum {
	AVP_User_Name                     =    1,
	AVP_Class                         =   25,
	AVP_Session_Timeout               =   27,
	AVP_Proxy_State                   =   33,
	AVP_Host_IP_Address               =  257,
	AVP_Auth_Application_Id            =  258,
	AVP_Vendor_Specific_Application_Id=  260,
	AVP_Redirect_Max_Cache_Time       =  262,
	AVP_Session_Id                    =  263,
	AVP_Origin_Host                   =  264,
	AVP_Supported_Vendor_Id           =  265,
	AVP_Vendor_Id                     =  266,
	AVP_Result_Code                   =  268,
	AVP_Product_Name                  =  269,
	AVP_Session_Binding               =  270,
	AVP_Disconnect_Cause              =  273,
	AVP_Auth_Request_Type             =  274,
	AVP_Auth_Grace_Period             =  276,
	AVP_Auth_Session_State            =  277,
	AVP_Origin_State_Id               =  278,
	AVP_Proxy_Host                    =  280,
	AVP_Error_Message                 =  281,
	AVP_Record_Route                  =  282,
	AVP_Destination_Realm             =  283,
	AVP_Proxy_Info                    =  284,
	AVP_Re_Auth_Request_Type          =  285,
	AVP_Authorization_Lifetime        =  291,
	AVP_Redirect_Host                 =  292,
	AVP_Destination_Host              =  293,
	AVP_Termination_Cause             =  295,
	AVP_Origin_Realm                  =  296,
/* begin SIP AAA with DIAMETER*/
	AVP_Resource					  =  400,
	AVP_Response					  =  401,	
	AVP_Challenge					  =  402,
	AVP_Method						  =  403,
	AVP_Service_Type				  =  404,
	AVP_User_Group					  =  405,
	AVP_SIP_MSGID					  =  406	

/* end SIP AAA with DIAMETER */
}AAA_AVPCodeNr;


/*   The following type allows the client to specify which direction to
 *   search for an AVP in the AVP list: */
typedef enum {
	AAA_FORWARD_SEARCH = 0,
	AAA_BACKWARD_SEARCH
} AAASearchType;



typedef enum {
	AAA_ACCT_EVENT = 1,
	AAA_ACCT_START = 2,
	AAA_ACCT_INTERIM = 3,
	AAA_ACCT_STOP = 4
} AAAAcctMessageType;


typedef enum {
	AVP_DUPLICATE_DATA,
	AVP_DONT_FREE_DATA,
	AVP_FREE_DATA,
} AVPDataStatus;

/* The following structure contains a message AVP in parsed format */
typedef struct avp {
	struct avp *next;
	struct avp *prev;
	enum {
		AAA_RADIUS,
		AAA_DIAMETER
	} packetType;
	AAA_AVPCode code;
	AAA_AVPFlag flags;
	AAA_AVPDataType type;
	AAAVendorId vendorId;
	str data;
	unsigned char free_it;
} AAA_AVP;


/* The following structure is used for representing lists of AVPs on the
 * message: */
typedef struct _avp_list_t {
	AAA_AVP *head;
	AAA_AVP *tail;
} AAA_AVP_LIST;


/* The following structure contains the full AAA message: */
typedef struct _message_t {
	AAAMsgFlag          flags;
	AAACommandCode      commandCode;
	AAAApplicationId    applicationId;
	AAAMsgIdentifier    endtoendId;
	AAAMsgIdentifier    hopbyhopId;
	AAASessionId        *sId;
	AAA_AVP             *sessionId;
	AAA_AVP             *orig_host;
	AAA_AVP             *orig_realm;
	AAA_AVP             *dest_host;
	AAA_AVP             *dest_realm;
	AAA_AVP             *res_code;
	AAA_AVP             *auth_ses_state;
	AAA_AVP_LIST        avpList;
	str                 buf;
	void                *in_peer;
} AAAMessage;




/**************************** AAA MESSAGE FUNCTIONS **************************/

/* MESSAGES
 */

#define is_req(_msg_) \
	(((_msg_)->flags)&0x80)

AAAMessage *AAAInMessage(
		AAACommandCode commandCode,
		AAAApplicationId appId);

AAAReturnCode AAAFreeMessage(
		AAAMessage **message);

AAAReturnCode AAASetMessageResultCode(
		AAAMessage *message,
		AAAResultCode resultCode);

void AAAPrintMessage(
		AAAMessage *msg);

AAAReturnCode AAABuildMsgBuffer(
		AAAMessage *msg );

AAAMessage* AAATranslateMessage(
		unsigned char* source,
		unsigned int sourceLen,
		int attach_buf );


/* AVPS
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
		unsigned int length,
		AVPDataStatus data_status);

AAA_AVP* AAACloneAVP(
		AAA_AVP *avp,
		unsigned char duplicate_data );

AAAReturnCode AAAAddAVPToMessage(
		AAAMessage *msg,
		AAA_AVP *avp,
		AAA_AVP *position);

AAA_AVP *AAAFindMatchingAVP(
		AAAMessage *msg,
		AAA_AVP *startAvp,
		AAA_AVPCode avpCode,
		AAAVendorId vendorId,
		AAASearchType searchType);

AAAReturnCode AAARemoveAVPFromMessage(
		AAAMessage *msg,
		AAA_AVP *avp);

AAAReturnCode AAAFreeAVP(
		AAA_AVP **avp);

AAA_AVP* AAAGetFirstAVP(
		AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetLastAVP(
		AAA_AVP_LIST *avpList);

AAA_AVP* AAAGetNextAVP(
		AAA_AVP *avp);

AAA_AVP* AAAGetPrevAVP(
		AAA_AVP *avp);

char *AAAConvertAVPToString(
		AAA_AVP *avp,
		char *dest,
		unsigned int destLen);


#endif
