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

#ifndef DIAMETER_H_
#define DIAMETER_H_


#include "utils.h"
#include <ctype.h>

#include "diameter_code_result.h"
#include "diameter_code_avp.h"

#define get_2bytes(_b) \
	((((unsigned char)(_b)[0])<<8)|\
	 (((unsigned char)(_b)[1])))

#define get_3bytes(_b) \
	((((unsigned char)(_b)[0])<<16)|(((unsigned char)(_b)[1])<<8)|\
	(((unsigned char)(_b)[2])))

#define get_4bytes(_b) \
	((((unsigned char)(_b)[0])<<24)|(((unsigned char)(_b)[1])<<16)|\
	(((unsigned char)(_b)[2])<<8)|(((unsigned char)(_b)[3])))

#define set_2bytes(_b,_v) \
	{(_b)[0]=((_v)&0x0000ff00)>>8;\
	(_b)[1]=((_v)&0x000000ff);}

#define set_3bytes(_b,_v) \
	{(_b)[0]=((_v)&0x00ff0000)>>16;(_b)[1]=((_v)&0x0000ff00)>>8;\
	(_b)[2]=((_v)&0x000000ff);}

#define set_4bytes(_b,_v) \
	{(_b)[0]=((_v)&0xff000000)>>24;(_b)[1]=((_v)&0x00ff0000)>>16;\
	(_b)[2]=((_v)&0x0000ff00)>>8;(_b)[3]=((_v)&0x000000ff);}

#define to_32x_len( _len_ ) \
	( (_len_)+(((_len_)&3)?4-((_len_)&3):0) )
	
	
/* AAA TYPES */

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

/* mesage codes */
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


typedef unsigned int    AAACommandCode;		/**< Code for a Diameter Command 	*/
typedef unsigned int    AAAVendorId;		/**< Vendor identifier				*/
typedef unsigned int    AAAExtensionId;		/**< Extension identifier			*/
typedef unsigned int    AAA_AVPCode;		/**< Code for an AVP				*/
typedef unsigned int    AAAValue;			/**< Value							*/
typedef unsigned int    AAAApplicationId;	/**< Application Identifier 		*/
typedef void*           AAAApplicationRef;	/**< Application Reference 			*/
typedef str             AAASessionId;		/**< Session Identifier				*/
typedef unsigned int    AAAMsgIdentifier;	/**< Message Identifier				*/
typedef unsigned char   AAAMsgFlag;			/**< Message flag					*/

#define Flag_Request 	0x80
#define Flag_Proxyable  0x40

#define Code_CE 	257
#define Code_DW 	280
#define Code_DP 	282
	

/** Status codes returned by functions in the AAA API */
typedef enum {
	AAA_ERR_NOT_FOUND = -2,         /**< handle or id not found */
	AAA_ERR_FAILURE   = -1,         /**< unspecified failure during an AAA op. */
	AAA_ERR_SUCCESS   =  0,         /**< AAA operation succeeded */
	AAA_ERR_NOMEM,                  /**< op. caused memory to be exhausted */
	AAA_ERR_PROTO,                  /**<  AAA protocol error */
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


/** The following are AVP data type codes. They correspond directly to
 * the AVP data types outline in the Diameter specification [1]: */
typedef enum {
	AAA_AVP_DATA_TYPE,
	AAA_AVP_STRING_TYPE,
	AAA_AVP_ADDRESS_TYPE,
	AAA_AVP_INTEGER32_TYPE,
	AAA_AVP_INTEGER64_TYPE,
	AAA_AVP_TIME_TYPE,
} AAA_AVPDataType;


/** The following are used for AVP header flags and for flags in the AVP
 *  wrapper struct and AVP dictionary definitions. */
typedef enum {
	AAA_AVP_FLAG_NONE               = 0x00,
	AAA_AVP_FLAG_MANDATORY          = 0x40,
	AAA_AVP_FLAG_RESERVED           = 0x1F,
	AAA_AVP_FLAG_VENDOR_SPECIFIC    = 0x80,
	AAA_AVP_FLAG_END_TO_END_ENCRYPT = 0x20,
} AAA_AVPFlag;


/** List with all known application identifiers */
typedef enum {
	AAA_APP_DIAMETER_COMMON_MSG  = 0,
	AAA_APP_NASREQ               = 1,
	AAA_APP_MOBILE_IP            = 2,
	AAA_APP_DIAMETER_BASE_ACC    = 3,
	AAA_APP_RELAY                = 0xffffffff,
}AAA_APP_IDS;


/**   The following type allows the client to specify which direction to
 *   search for an AVP in the AVP list: */
typedef enum {
	AAA_FORWARD_SEARCH = 0,	/**< search forward 	*/
	AAA_BACKWARD_SEARCH		/**< search backwards 	*/
} AAASearchType;

/** Hint on what do do with the AVP payload */
typedef enum {
	AVP_DUPLICATE_DATA,		/**< Duplicate the payload; the source can be safely removed at any time */
	AVP_DONT_FREE_DATA,		/**< Don't duplicate and don't free; the source will always be there. */
	AVP_FREE_DATA,			/**< Don't duplicate, but free when done; this is the only reference to source. */
} AVPDataStatus;

/** This structure contains a message AVP in parsed format */
typedef struct avp {
	struct avp *next;		/**< next AVP if in a list 				*/
	struct avp *prev;		/**< previous AVP if in a list 			*/
	AAA_AVPCode code;		/**< AVP code 							*/
	AAA_AVPFlag flags;		/**< AVP flags 							*/
	AAA_AVPDataType type;	/**< AVP payload type 					*/
	AAAVendorId vendorId;	/**< AVP vendor id 						*/
	str data;				/**< AVP payload						*/
	unsigned char free_it;	/**< if to free the payload when done	*/
} AAA_AVP;


/**
 * This structure is used for representing lists of AVPs on the
 * message or in grouped AVPs. */
typedef struct _avp_list_t {
	AAA_AVP *head;			/**< The first AVP in the list 	*/
	AAA_AVP *tail;			/**< The last AVP in the list 	*/
} AAA_AVP_LIST;


/** This structure contains the full AAA message. */
typedef struct _message_t {
	AAACommandCode      commandCode;	/**< command code for the message */
	AAAMsgFlag          flags;			/**< flags */
	AAAApplicationId    applicationId;	/**< application identifier */
	AAAMsgIdentifier    endtoendId;		/**< End-to-end identifier */
	AAAMsgIdentifier    hopbyhopId;		/**< Hop-by-hop identitfier */
	AAA_AVP		       	*sessionId;		/**< SessionId 				*/
	AAA_AVP             *orig_host;		/**< shortcut to Origin Host AVP */
	AAA_AVP             *orig_realm;	/**< shortcut to Origin Realm AVP */
	AAA_AVP             *dest_host;		/**< shortcut to Destination Host AVP */
	AAA_AVP             *dest_realm;	/**< shortcut to Destination Realm AVP */
	AAA_AVP             *res_code;		/**< shortcut to Result Code AVP */
	AAA_AVP             *auth_ses_state;/**< shortcut to Authorization Session State AVP */
	AAA_AVP_LIST        avpList;		/**< list of AVPs in the message */
	str                 buf;			/**< Diameter network representation */
	void                *in_peer;		/**< Peer that this message was received from */
} AAAMessage;




/**************************** AAA MESSAGE FUNCTIONS **************************/

/* MESSAGES */

/** if the message is a request */
#define is_req(_msg_) \
	(((_msg_)->flags)&0x80)



/*************************** AAA Transactions ********************************/
/**
 * This structure defines a Diameter Transaction.
 * This is used to link a response to a request
 */
typedef struct _AAATransaction{
	unsigned int hash,label;
	AAAApplicationId application_id;
	AAACommandCode command_code;
} AAATransaction;


/** Function for callback on transaction events: response or time-out for request. */
typedef void (AAATransactionCallback_f)(int is_timeout,void *param,AAAMessage *ans, long elapsed_msecs);
/** Function for callback on request received */
typedef AAAMessage* (AAARequestHandler_f)(AAAMessage *req, void *param);
/** Function for callback on response received */
typedef void (AAAResponseHandler_f)(AAAMessage *res, void *param);


#endif /*DIAMETER_H_*/
