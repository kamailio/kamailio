/*
 * ro_avp.h
 *
 *  Created on: 29 May 2014
 *      Author: jaybeepee
 */

#ifndef RO_AVP_H_
#define RO_AVP_H_

#include "../cdp/cdp_load.h"

int ro_add_destination_realm_avp(AAAMessage *msg, str data);
int Ro_add_avp(AAAMessage *m, char *d, int len, int avp_code, int flags, int vendorid, int data_do, const char *func);


#endif /* RO_AVP_H_ */
