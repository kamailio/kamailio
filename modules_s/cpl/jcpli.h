/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _JCPLI_H
#define _JCPLI_H

/**
 * Headers of functions to communicate with Java CPL Interpreter Server
 * and for processing the response
 */

/**
 * @author:       Daniel-Constantin MIERLA
 * @email:        mierla@fokus.fhg.de
 * @organization: MOBIS - FhI FOKUS, Berlin
 * @version:      0.1.0
 */

#include "../../dprint.h"
#include "../../mem/mem.h"


#define ACCEPT_CALL      1
#define REJECT_CALL      2
#define REDIRECT_CALL  3


/**
 * structure for storage the attributes of a reject message
 */
typedef struct _RejectMessage
{
	int status;
	int reasonLength;
	char *reason;
} TRejectMessage;

/**
 * structure for storage location attributes
 */
typedef struct _Location
{
	float priority;
	int urlLength;
	char *URL;
} TLocation;

/**
 * structure for storage the attributes of a redirect message
 */
typedef struct _RedirectMessage
{
	int permanent;
	int numberOfLocations;
	TLocation *locations;
} TRedirectMessage;

/**
 * reverse the order of bytes of an integer
 */
int reverseInteger(int val);

/**
 * redMessage->locations[i].URL
 */
void freeRejectMessage(TRejectMessage *rejMessage);

/**
 * free space allocated for that structure
 */
void freeRedirectMessage(TRedirectMessage *redMessage);

/**
 *
 */
void printRejectMessage(TRejectMessage *rejMessage);

/**
 *
 */
void printRedirectMessage(TRedirectMessage *redMessage);

/**
 * parse the message received for a REJECT_CALL
 */
TRejectMessage* parseRejectResponse(char *message, int length);

/**
 * parse the message received for a REDIRECT_CALL
 */
TRedirectMessage* parseRedirectResponse(char *message, int length);

/**
 *
 */
void processResponseMessage(char* msgBuff, int msgLen, int msgType);

/**
 * process SIP message through Java CPL Interpreter
 * return CPL decision
 */
int executeCPLForSIPMessage(char *msgContent, int msgLength, char *serverAddress, int serverPort, char **pRespBuff, int *pRespLen);

#endif



