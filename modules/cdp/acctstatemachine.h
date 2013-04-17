/*
 * acctstatemachine.h
 *
 *  Created on: 03 Apr 2013
 *      Author: jaybeepee
 */

#ifndef ACCTSTATEMACHINE_H_
#define ACCTSTATEMACHINE_H_

#include "diameter_api.h"
#include "session.h"
#include "config.h"

#define EPOCH_UNIX_TO_EPOCH_NTP 2208988800u // according to http://www.cis.udel.edu/~mills/y2k.html

inline int cc_acc_client_stateful_sm_process(cdp_session_t* cc_acc, int event, AAAMessage* msg);

#endif /* ACCTSTATEMACHINE_H_ */
