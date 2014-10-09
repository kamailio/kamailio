/*
 * mod.h
 *
 *  Created on: 21 Feb 2013
 *      Author: jaybeepee
 */

#ifndef MOD_H_
#define MOD_H_

#define MOD_NAME "ims_charging"

#define RO_CC_START 	1
#define RO_CC_INTERIM 	2
#define RO_CC_STOP 		3

#define RO_UNKNOWN_DIRECTION -1
#define RO_ORIG_DIRECTION 0
#define RO_TERM_DIRECTION 1

/** Return and break the execution of routing script */
#define RO_RETURN_BREAK	0
/** Return true in the routing script */
#define RO_RETURN_TRUE	1
#define RO_RETURN_TRUE_STR "1"
/** Return false in the routing script */
#define RO_RETURN_FALSE -1
#define RO_RETURN_FALSE_STR "-1"
/** Return error in the routing script */
#define RO_RETURN_ERROR -2
#define RO_RETURN_ERROR_STR "-2"

/** Diameter Termination Cause Codes */
#define TERM_CAUSE_LOGOUT 1
#define TERM_CAUSE_SERVICE_NOT_PROVIDED 2
#define TERM_CAUSE_BAD_ANSWER 3
#define TERM_CAUSE_ADMINISTRATIVE 4
#define TERM_CAUSE_LINK_BROKEN 5
#define TERM_CAUSE_AUTH_EXPIRED 6
#define TERM_CAUSE_USER_MOVED 7
#define TERM_CAUSE_SESSION_TIMEOUT 8



#define RO_AVP_CCA_RETURN_CODE "cca_return_code"
#define RO_AVP_CCA_RETURN_CODE_LENGTH 15

#define RO_MAC_AVP_NAME	"$avp(ro_mac_value)"

#define DB_DEFAULT_UPDATE_PERIOD	60

#define DB_MODE_NONE				0
#define DB_MODE_REALTIME			1
#define DB_MODE_SHUTDOWN			2

#endif /* MOD_H_ */
