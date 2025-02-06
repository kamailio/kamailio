/*
 * Copyright (C) 2024 Dragos Vingarzan (neatpath.net)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include "auth_vector.h"

#include "milenage.h"

#include "../../core/mem/shm_mem.h"


// SQN should be incremented after each new vector generation.
auth_vector *auth_vector_make_local(uint8_t k[16], uint8_t op[16], int opIsOPc,
		uint8_t amf[2], uint8_t sqn[6])
{
	uint8_t rand[16];
	uint8_t op_c[16];
	uint8_t mac_a[8];
	uint8_t res[8];
	uint8_t ck[16];
	uint8_t ik[16];
	uint8_t ak[6];
	uint8_t authenticate[16 + 6 + 2 + 8];

	// f0 - generate random
	f0(rand);

	// Compute OPc from K and OP, if not already provided
	if(opIsOPc == 0)
		ComputeOPc(op_c, k, op);
	else
		memcpy(op_c, op, 16);

	// f1 - calculate MAC-A
	f1(mac_a, k, rand, sqn, amf, op_c);

	// f2345 - calculate RES, CK, IK, AK
	f2345(res, ck, ik, ak, k, op_c, rand);

	// AUTN = SQN ^ AK || AMF || MAC-A
	// Authenticate = RAND || AUTN
	memcpy(authenticate, rand, 16);
	for(int i = 0; i < 6; i++)
		authenticate[16 + i] = sqn[i] ^ ak[i];
	memcpy(authenticate + 22, amf, 2);
	memcpy(authenticate + 24, mac_a, 8);

	str s_auth_scheme = {.s = "Digest-AKAv1-MD5", .len = 16};
	str s_authenticate = {.s = (char *)authenticate, .len = 16 + 6 + 2 + 8};
	str s_authorization = {.s = (char *)res, .len = 8};
	str s_ck = {.s = (char *)ck, .len = 16};
	str s_ik = {.s = (char *)ik, .len = 16};

	auth_vector *av = new_auth_vector(
			1, s_auth_scheme, s_authenticate, s_authorization, s_ck, s_ik);
	if(av == NULL) {
		return NULL;
	}
	av->is_locally_generated = 1;
	return av;
}


int auth_vector_resync_local(uint8_t sqnMSout[6], auth_vector *av,
		uint8_t auts[14], uint8_t k[16], uint8_t op[16], int opIsOPc,
		uint8_t amf[2])
{
	uint8_t rand[16];
	uint8_t op_c[16];
	uint8_t ak[6];
	uint8_t sqnMS[6];
	uint8_t mac_s[8];
	uint8_t xmac_s[8];

	if(!av->is_locally_generated) {
		LM_ERR("auth_vector is not locally generated - let the HSS handle "
			   "resync\n");
		return -1;
	}

	// Extract RAND from the sent authenticate
	if(av->authenticate.len < 16 + 6 + 2 + 8) {
		LM_ERR("auth_vector authenticate len %d is too short\n",
				av->authenticate.len);
		return -1;
	}
	memcpy(rand, av->authenticate.s, 16);

	// Compute OPc from K and OP, if not already provided
	if(opIsOPc == 0)
		ComputeOPc(op_c, k, op);
	else
		memcpy(op_c, op, 16);

	// Compute AK
	f5star(ak, k, op_c, rand);

	// Unpack the AUTS = (SQN_MS ^ AK) || MAC-S
	for(int i = 0; i < 6; i++)
		sqnMS[i] = auts[i] ^ ak[i];
	memcpy(mac_s, auts + 6, 8);

	// Compute XMAC-S
	f1star(xmac_s, k, op_c, rand, sqnMS, amf);

	// Check if MAC-S == XMAC-S
	if(memcmp(mac_s, xmac_s, 8) != 0) {
		LM_ERR("auth_vector resync failed\n");
		return -2;
	}

	// Return the SQN-MS
	memcpy(sqnMSout, sqnMS, 6);

	return 0;
}

void sqn_increment(uint8_t sqn[6])
{
	for(int i = 5; i >= 0; i--) {
		if(sqn[i] == 0xFF) {
			sqn[i] = 0;
		} else {
			sqn[i]++;
			return;
		}
	}
}
