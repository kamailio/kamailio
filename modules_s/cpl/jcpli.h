/*
 * $Id$
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



