/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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


/**
 * Functions for communication with Java CPL Interpreter Server
 * and for processing the response
 */

/**
 * @author:       Daniel-Constantin MIERLA
 * @email:        mierla@fokus.fhg.de
 * @organization: MOBIS - FhI FOKUS, Berlin
 * @version:      0.1.0
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<sys/un.h>
#include <unistd.h>

#include "jcpli.h"

/**
 * reverse the order of bytes of an integer
 */
int reverseInteger(int val)
{
	return (((val >> 24) & 0xff) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000) | ((val << 24) & 0xff000000));
}

/**
 * free space allocated for a TRejectMessage structure
 */
void freeRejectMessage(TRejectMessage *rejMessage)
{
	pkg_free(rejMessage->reason);
    pkg_free(rejMessage);
}

/**
 * free space allocated for a TRedirectMessage structure
 */
void freeRedirectMessage(TRedirectMessage *redMessage)
{
    int i;
    for(i = 0; i < redMessage->numberOfLocations; i++)
    	pkg_free(redMessage->locations[i].URL);
    pkg_free(redMessage->locations);
    pkg_free(redMessage);
}

/**
 * print to the standard output the content of a Reject Response Message
 */
void printRejectMessage(TRejectMessage *rejMessage)
{
    DBG("DEBUG : --- Response: REJECT CALL ---\n\tStatus: %d \n\t"
     "Reason length: %d \n\tReason: %s\n", rejMessage->status,
     rejMessage->reasonLength,rejMessage->reason);
}

/**
 * print to the standard output the content of a Redirect Response Message
 */
void printRedirectMessage(TRedirectMessage *redMessage)
{
    int i;

    DBG("--- Response: REDIRECT CALL ---\n");
    DBG("\tPermanent: %d\n", redMessage->permanent);
    DBG("\tNumber of locations: %d\n", redMessage->numberOfLocations);
    for(i = 0; i < redMessage->numberOfLocations; i++)
    {
    	DBG("\t--- Location <%d>\n", i);
        DBG("\tPriority: %3.1f\n", redMessage->locations[i].priority);
    	DBG("\tURL length: %d\n", redMessage->locations[i].urlLength);
    	DBG("\tURL: %s\n", redMessage->locations[i].URL);
    }
}

/**
 * parse the message received for a REJECT_CALL
 */
TRejectMessage* parseRejectResponse(char *message, int length)
{
	TRejectMessage* rejMessage;

    rejMessage = (TRejectMessage*)pkg_malloc(sizeof(TRejectMessage));

    rejMessage->status = reverseInteger(*((int*)message));
    rejMessage->reasonLength = reverseInteger(*((int*)(message+sizeof(int))));
    rejMessage->reason = (char*)pkg_malloc(rejMessage->reasonLength + 1);
    memcpy(rejMessage->reason, message + 2*sizeof(int), rejMessage->reasonLength);

	rejMessage->reason[rejMessage->reasonLength] = 0;

    return rejMessage;
}

/**
 * parse the message received for a REDIRECT_CALL
 */
TRedirectMessage* parseRedirectResponse(char *message, int length)
{
	int i;
    char* p;
    TRedirectMessage* redMessage;

    redMessage = (TRedirectMessage*)pkg_malloc(sizeof(TRedirectMessage));

    redMessage->permanent = reverseInteger(*((int*)message));
    redMessage->numberOfLocations = reverseInteger(*((int*)(message+sizeof(int))));
    redMessage->locations = (TLocation*)pkg_malloc(redMessage->numberOfLocations * sizeof(TLocation));

    p = message + 2*sizeof(int);
    for(i = 0; i < redMessage->numberOfLocations; i++)
    {
    	redMessage->locations[i].priority = (float)reverseInteger(*((int*)p)) / 10;
    	redMessage->locations[i].urlLength = reverseInteger(*((int*)(p + sizeof(int))));

	    redMessage->locations[i].URL = (char*)pkg_malloc(redMessage->locations[i].urlLength + 1);
    	memcpy(redMessage->locations[i].URL, p + 2*sizeof(int), redMessage->locations[i].urlLength);
	    redMessage->locations[i].URL[redMessage->locations[i].urlLength] = 0;

        p += 2*sizeof(int) + redMessage->locations[i].urlLength;
    }

    return redMessage;
}

/**
 * process a response message, depending on type of repose, and print the values of fields from message
 */
void processResponseMessage(char* msgBuff, int msgLen, int msgType)
{
    TRejectMessage* rejMessage;
    TRedirectMessage* redMessage;

    switch(msgType)
    {
	case ACCEPT_CALL: // accept call response
		DBG("DEBUG : processResponseMessage :  Response: ACCEPT CALL\n");
		break;
	case REJECT_CALL: // reject call response
                rejMessage = parseRejectResponse(msgBuff, msgLen);
                printRejectMessage(rejMessage);

                freeRejectMessage(rejMessage);
    		break;
    	case REDIRECT_CALL: // redirect call response
                redMessage = parseRedirectResponse(msgBuff, msgLen);
                printRedirectMessage(redMessage);

                freeRedirectMessage(redMessage);
		break;
	default:
		DBG("DEBUG : processResponseMessage : Response: UNKNOWN\n");
    }

} // END processResponseMessage(...)

/**
 * process SIP message through Java CPL Interpreter
 * return CPL decision
 */
int executeCPLForSIPMessage(char *msgContent, int msgLength, char *serverAddress, int serverPort, char **pRespBuff, int *pRespLen)
{
    int jciSocket, resp, n;
    struct sockaddr_in address;
    struct hostent *he;

	int respLen;
	char *respBuff;

    respLen = 0;
    resp = 0;
    respBuff = NULL;

    // create the socket
    if((jciSocket = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
	DBG("DEBUG : executeCPLForSIPMessage : Error to create the socket\n");
        return 0;
    }

    // get the information about JCI Server address
    he=gethostbyname(serverAddress);
    if(he == NULL)
    {
	DBG("DEBUG : executeCPLForSIPMessage : Error to get the information about JCI Server address\n");
        return 0;
    }

    // fill the fields of the address
    memcpy(&address.sin_addr, he->h_addr, he->h_length);
    address.sin_family=AF_INET;
    address.sin_port=htons(serverPort);

    // try to connect with JCI server
    if (connect(jciSocket, (struct sockaddr *)&address, sizeof(address))<0)
    {
    	DBG("DEBUG : executeCPLForSIPMessage : Error to connect with JCI Server\n");
        return 0;
    }


	// reverse the integer to match the internal representation
	// of an integer in Java
    n = reverseInteger(msgLength);

    // send the SIP message to the JCI server
	send(jciSocket, &n, sizeof(int), 0);
	send(jciSocket, msgContent, msgLength, 0);

	// receive the response from server
	n=recv(jciSocket, &resp, sizeof(int), MSG_WAITALL);

    resp = reverseInteger(resp);

	DBG("DEBUG : executeCPLForSIPMessage : Result: %d \n", resp);

    // receive the length of the rest of message
    n = recv(jciSocket, &respLen, sizeof(int), MSG_WAITALL);
    // reverse the length of message
    respLen = reverseInteger(respLen);
    DBG("DEBUG : executeCPLForSIPMessage : Message length: %d\n", respLen);
    // receive the whole message
    if(respLen != 0)
    {
    	respBuff = (char*)pkg_malloc(respLen*1);
        n = recv(jciSocket, respBuff, respLen, MSG_WAITALL);
        DBG("DEBUG : executeCPLForSIPMessage : Bytes received: %d\n", n);
        if(n != respLen) // some errors occurred
        	return 0;
	}

    if(pRespBuff == NULL || pRespLen == NULL)
    {
    	// free the space of the message buffer
        if(respBuff != NULL)
        	pkg_free(respBuff);
    }
    else
    {
    	*pRespBuff = respBuff;
        *pRespLen = respLen;
    }

    // close the socket
    close(jciSocket);

    return resp;
} // END executeCPLForSIPMessage( ... )
