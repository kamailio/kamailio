/*
 *
 * Copyright (C) 2015 ng-voice GmbH, Carsten Bock, carsten@ng-voice.com
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

#ifndef OCS_AVP_HELPER_H
#define OCS_AVP_HELPER_H

#include "../cdp/diameter_api.h"

str get_avp(AAAMessage *msg, int avp_code, int vendor_id, const char *func);
str getSession(AAAMessage *msg);
int getRecordNummber(AAAMessage *msg);
str getSubscriptionId1(AAAMessage *msg, int *type);
int isOrig(AAAMessage *msg);
str getCalledParty(AAAMessage *msg);
int getUnits(AAAMessage *msg, int *used, int *service, int *group);
str getAccessNetwork(AAAMessage *msg);

int ocs_build_answer(AAAMessage *ccr, AAAMessage *cca, int result_code,
		int granted_units, int final_unit);

#endif /* OCS_AVP_HELPER_H */
