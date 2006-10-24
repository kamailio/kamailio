/* domainpolicy.h v 0.1 2002/12/27
 *
 * Header file for domainpolicy table relates functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef DOMAINPOLICY_H
#define DOMAINPOLICY_H
		

#include "../../parser/msg_parser.h"

#define MAX_DOMAIN_SIZE 512

/*
 * how many indirect DDDS naptr are we going to follow?
 */
#define MAX_DDDS_RECURSIONS 5


/*
 * dp_can_connect return codes
 */
#define DP_DDDS_RET_DNSERROR -2
#define DP_DDDS_RET_NEGATIVE -1  /* negative values means false, 0 stops execution and discards the SIP request */
#define DP_DDDS_RET_POSITIVE 1
#define DP_DDDS_RET_NOTFOUND 2

/*
 * Check if host in Request URI has DP-DDDS NAPTRs and if we can connect to them
 */
int dp_can_connect(struct sip_msg* _msg, char* _s1, char* _s2);

/*
 * Apply DP-DDDS policy to current SIP message. This means
 * build a new destination URI from the policy AVP and export it
 * as AVP. Then in openser.cfg this new target AVP can be pushed
 * into the destination URI $duri
 */
int dp_apply_policy(struct sip_msg* _msg, char* _s1, char* _s2);

int domainpolicy_db_bind(char* db_url);
int domainpolicy_db_init(char* db_url);
void domainpolicy_db_close();
int domainpolicy_db_ver(char* db_url, str* name);


#endif /* DOMAINPOLICY_H */
