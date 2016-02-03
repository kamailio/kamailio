/**
 * Copyright (C) 2016 Daniel-Constantin Mierla (asipto.com)
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
 */

/*!
 * \file
 * \brief Kamailio topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../dprint.h"
#include "../../hashes.h"
#include "../../locking.h"
#include "../../parser/parse_uri.h"

#include "../../lib/srdb1/db.h"
#include "../../lib/srutils/sruid.h"

#include "tps_storage.h"

extern sruid_t _tps_sruid;

#define TPS_STORAGE_LOCK_SIZE	1<<8
static gen_lock_set_t *_tps_storage_lock_set = NULL;

/**
 *
 */
int tps_storage_lock_set_init(void)
{
	_tps_storage_lock_set = lock_set_alloc(TPS_STORAGE_LOCK_SIZE);
	if(_tps_storage_lock_set==NULL
			|| lock_set_init(_tps_storage_lock_set)==NULL) {
		LM_ERR("cannot initiate lock set\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int tps_storage_lock_get(str *lkey)
{
	uint32_t pos;
	pos = core_case_hash(lkey, 0, TPS_STORAGE_LOCK_SIZE);
	LM_DBG("tps lock get: %u\n", pos);
	lock_set_get(_tps_storage_lock_set, pos);
	return 1;
}

/**
 *
 */
int tps_storage_lock_release(str *lkey)
{
	uint32_t pos;
	pos = core_case_hash(lkey, 0, TPS_STORAGE_LOCK_SIZE);
	LM_DBG("tps lock release: %u\n", pos);
	lock_set_release(_tps_storage_lock_set, pos);
	return 1;
}

/**
 *
 */
int tps_storage_lock_set_destroy(void)
{
	if(_tps_storage_lock_set!=NULL) {
		lock_set_destroy(_tps_storage_lock_set);
		lock_set_dealloc(_tps_storage_lock_set);
	}
	_tps_storage_lock_set = NULL;
	return 0;
}

/**
 *
 */
int tps_storage_dialog_find(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_dialog_save(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_dialog_rm(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}


/**
 *
 */
int tps_storage_branch_find(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_branch_save(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_branch_rm(sip_msg_t *msg, tps_data_t *td)
{
	return 0;
}

/**
 *
 */
int tps_storage_fill_contact(sip_msg_t *msg, tps_data_t *td, int dir)
{
	str sv;
	sip_uri_t puri;

	sruid_next(&_tps_sruid);

	if(dir==TPS_DIR_DOWNSTREAM) {
		sv = td->bs_contact;
	} else {
		sv = td->as_contact;
	}
	if(td->cp + 8 + (2*_tps_sruid.uid.len) + sv.len >= td->cbuf + TPS_DATA_SIZE) {
		LM_ERR("insufficient data buffer\n");
		return -1;
	}
	if (parse_uri(sv.s, sv.len, &puri) < 0) {
		LM_ERR("failed to parse the uri\n");
		return -1;
	}
	if(dir==TPS_DIR_DOWNSTREAM) {
		td->b_uuid.s = td->cp;
		*td->cp = 'b';
		td->cp++;
		memcpy(td->cp, _tps_sruid.uid.s, _tps_sruid.uid.len);
		td->cp += _tps_sruid.uid.len;
		td->b_uuid.len = td->cp - td->b_uuid.s;

		td->bs_contact.s = td->cp;
	} else {
		td->a_uuid.s = td->cp;
		*td->cp = 'a';
		td->cp++;
		memcpy(td->cp, _tps_sruid.uid.s, _tps_sruid.uid.len);
		td->cp += _tps_sruid.uid.len;
		td->a_uuid.len = td->cp - td->a_uuid.s;

		td->as_contact.s = td->cp;
	}
	*td->cp = '<';
	td->cp++;
	if(dir==TPS_DIR_DOWNSTREAM) {
		*td->cp = 'b';
	} else {
		*td->cp = 'a';
	}
	td->cp++;
	memcpy(td->cp, _tps_sruid.uid.s, _tps_sruid.uid.len);
	td->cp += _tps_sruid.uid.len;
	*td->cp = '@';
	td->cp++;
	memcpy(td->cp, puri.host.s, puri.host.len);
	td->cp += puri.host.len;
	if(puri.port.len>0) {
		*td->cp = ':';
		td->cp++;
		memcpy(td->cp, puri.port.s, puri.port.len);
		td->cp += puri.port.len;
	}
	if(puri.transport_val.len>0) {
		memcpy(td->cp, ";transport=", 11);
		td->cp += 11;
		memcpy(td->cp, puri.transport_val.s, puri.transport_val.len);
		td->cp += puri.transport_val.len;
	}

	*td->cp = '>';
	td->cp++;
	if(dir==TPS_DIR_DOWNSTREAM) {
		td->bs_contact.len = td->cp - td->bs_contact.s;
	} else {
		td->as_contact.len = td->cp - td->as_contact.s;
	}
	return 0;
}

/**
 *
 */
int tps_storage_record(sip_msg_t *msg, tps_data_t *td)
{
	int ret;

	ret = tps_storage_fill_contact(msg, td, TPS_DIR_DOWNSTREAM);
	if(ret<0) return ret;
	return tps_storage_fill_contact(msg, td, TPS_DIR_UPSTREAM);
}
