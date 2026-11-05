/*
 * ims_qos_n5 - P-CSCF media authorisation over N5 (Npcf_PolicyAuthorization)
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
 */

#ifndef _IMS_QOS_N5_OAUTH_H_
#define _IMS_QOS_N5_OAUTH_H_

/* OAuth2 for the SBI (3GPP TS 29.500 clause 6.7, TS 29.510 clause 5.4):
 * the AF registers with the NRF as an NF instance and obtains access tokens
 * from it with AccessTokenReq (client credentials). */

int n5_oauth_mod_init(void);

/* NRF registration and heartbeat; runs in the module's timer process */
void n5_nrf_timer(unsigned int ticks, void *param);

/* Access token for the PCF's Npcf_PolicyAuthorization service, cached per
 * process; NULL if OAuth2 is disabled or no token could be obtained. */
const char *n5_oauth_pcf_token(void);

#endif
