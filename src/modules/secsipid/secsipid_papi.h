/**
 * Copyright (C) 2021 Daniel-Constantin Mierla (asipto.com)
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


#ifndef _SECSIPID_PAPI_H_
#define _SECSIPID_PAPI_H_

typedef struct secsipid_papi
{
	int (*SecSIPIDSignJSONHP)(char *headerJSON, char *payloadJSON,
			char *prvkeyPath, char **outPtr);

	int (*SecSIPIDGetIdentity)(char *origTN, char *destTN, char *attestVal,
			char *origID, char *x5uVal, char *prvkeyPath, char **outPtr);

	int (*SecSIPIDGetIdentityPrvKey)(char *origTN, char *destTN,
			char *attestVal, char *origID, char *x5uVal, char *prvkeyData,
			char **outPtr);

	int (*SecSIPIDCheck)(char *identityVal, int identityLen, int expireVal,
			char *pubkeyPath, int timeoutVal);

	int (*SecSIPIDCheckFull)(char *identityVal, int identityLen, int expireVal,
			char *pubkeyPath, int timeoutVal);

	int (*SecSIPIDCheckFullPubKey)(char *identityVal, int identityLen,
			int expireVal, char *pubkeyVal, int pubkeyLen);

	int (*SecSIPIDSetFileCacheOptions)(char *dirPath, int expireVal);

	int (*SecSIPIDGetURLContent)(
			char *urlVal, int timeoutVal, char **outPtr, int *outLen);

	int (*SecSIPIDOptSetS)(char *optName, char *optVal);

	int (*SecSIPIDOptSetN)(char *optName, int optVal);

	int (*SecSIPIDOptSetV)(char *optNameVal);

} secsipid_papi_t;

typedef int (*secsipid_proc_bind_f)(secsipid_papi_t *papi);

#endif
