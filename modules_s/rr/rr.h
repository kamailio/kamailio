/*
 * Route & Record-Route module
 *
 * $Id$
 */

#ifndef __RR_H__
#define __RR_H__

#include "../../msg_parser.h"


/*
 * Finds Route header field in a SIP message
 */
int findRouteHF(struct sip_msg* _m);


/*
 * Gets the first URI from the first Route
 * header field in a message
 */
int parseRouteHF(struct sip_msg* _m, char** _s, char** _next);


/*
 * Rewrites Request URI from Route HF
 */
int rewriteReqURI(struct sip_msg* _m, char* _s);


/*
 * Removes the first URI from the first Route header
 * field, if there is only one URI in the Route header
 * field, remove the whole header field
 */
int remFirstRoute(struct sip_msg* _m, char* _next);


/*
 * Builds Record-Route line
 */
int buildRRLine(struct sip_msg* _m, char* _l);


/*
 * Add a new Record-Route line in SIP message
 */
int addRRLine(struct sip_msg* _m, char* _l);




#endif
