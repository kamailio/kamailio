/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <secsipid.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/kemi.h"

#include "../secsipid/secsipid_papi.h"

MODULE_VERSION

int secsipid_proc_bind(secsipid_papi_t *papi)
{
	papi->SecSIPIDSignJSONHP = SecSIPIDSignJSONHP;
	papi->SecSIPIDGetIdentity = SecSIPIDGetIdentity;
	papi->SecSIPIDGetIdentityPrvKey = SecSIPIDGetIdentityPrvKey;
	papi->SecSIPIDCheck = SecSIPIDCheck;
	papi->SecSIPIDCheckFull = SecSIPIDCheckFull;
	papi->SecSIPIDCheckFullPubKey = SecSIPIDCheckFullPubKey;
	papi->SecSIPIDSetFileCacheOptions = SecSIPIDSetFileCacheOptions;
	papi->SecSIPIDGetURLContent = SecSIPIDGetURLContent;
	papi->SecSIPIDOptSetS = SecSIPIDOptSetS;
	papi->SecSIPIDOptSetN = SecSIPIDOptSetN;
	papi->SecSIPIDOptSetV = SecSIPIDOptSetV;

	return 0;
}
