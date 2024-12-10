/**
 * Copyright (C) 2024 kamailio.org
 * Copyright (C) 2017 net2phone.com
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
#include "../../core/dprint.h"
#include "../../core/ut.h"
#include "../../core/basex.h"
#include "../topos/api.h"
#include "topos_htable_storage.h"

extern topos_api_t _tps_api;
extern htable_api_t _tps_htable_api;
extern str _tps_htable_dialog;
extern str _tps_htable_transaction;
extern int _tps_base64;

static char _tps_htable_key_buf[TPS_HTABLE_SIZE_KEY];
static char _tps_htable_val_buf[TPS_HTABLE_SIZE_VAL];
static char _tps_base64_buf[TPS_BASE64_ROWS][TPS_BASE64_SIZE];

/***		HTABLE HELPER FUNCTIONS		***/

/**
 * Inserts key/value from global buffers, into htable module table
 */
static int helper_htable_insert(str table)
{
	int ret = 0;
	str hkey;
	int_str hval;

	hkey.s = _tps_htable_key_buf;
	hkey.len = strlen(_tps_htable_key_buf);

	hval.s.s = _tps_htable_val_buf;
	hval.s.len = strlen(_tps_htable_val_buf);

	LM_DBG("insert into table=%.*s, key=%.*s, value=%.*s", table.len, table.s,
			hkey.len, hkey.s, hval.s.len, hval.s.s);
	ret = _tps_htable_api.set(&table, &hkey, AVP_VAL_STR, &hval, 1);
	if(ret < 0) {
		LM_ERR("failed to insert str, using htable module api\n");
		return -1;
	}

	return 0;
}

/**
 * Sets expire for key/value from global buffers
 */
static int helper_htable_set_expire(str table, int n)
{
	int ret = 0;
	str hkey;
	int_str hval;

	hkey.s = _tps_htable_key_buf;
	hkey.len = strlen(_tps_htable_key_buf);

	hval.n = n;

	LM_DBG("set expire for table=%.*s, key=%.*s, value=%d", table.len, table.s,
			hkey.len, hkey.s, n);
	ret = _tps_htable_api.set_expire(&table, &hkey, 0, &hval);
	if(ret < 0) {
		LM_ERR("failed to set expire, using htable module api\n");
		return -1;
	}

	return 0;
}


/**
 *		TRANSACTION/DIALOG HELPER FUNCTIONS
 */
static int tps_htable_insert_initial_method_branch(
		tps_data_t *md, tps_data_t *sd)
{
	char *ptr;
	int ret = 0, expire = 0;
	unsigned long rectime = 0;
	str xuuid = str_init("");

	// checks
	if(md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(md->x_vbranch1.len <= 0) {
		LM_DBG("no via branch for this message\n");
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	if(md->a_uuid.len > 1) {
		xuuid.s = md->a_uuid.s + 1;
		xuuid.len = md->a_uuid.len - 1;
	} else if(md->b_uuid.len > 1) {
		xuuid.s = md->b_uuid.s + 1;
		xuuid.len = md->b_uuid.len - 1;
	} else if(sd->a_uuid.len > 1) {
		xuuid.s = sd->a_uuid.s + 1;
		xuuid.len = sd->a_uuid.len - 1;
	} else if(sd->b_uuid.len > 1) {
		xuuid.s = sd->b_uuid.s + 1;
		xuuid.len = sd->b_uuid.len - 1;
	}

	ptr = _tps_htable_key_buf;

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(md->s_method.s, md->s_method.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(md->a_callid.s, md->a_callid.len, _tps_base64_buf[1],
				TPS_BASE64_SIZE - 1);
		base64url_enc(md->b_tag.s, md->b_tag.len, _tps_base64_buf[2],
				TPS_BASE64_SIZE - 1);
		base64url_enc(
				xuuid.s, xuuid.len, _tps_base64_buf[3], TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s|%s|%s|x%s",
				_tps_base64_buf[0], _tps_base64_buf[1], _tps_base64_buf[2],
				_tps_base64_buf[3]);

	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s|%.*s|%.*s|x%.*s",
				md->s_method.len, md->s_method.s, md->a_callid.len,
				md->a_callid.s, md->b_tag.len, md->b_tag.s, xuuid.len, xuuid.s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}


	// build value
	rectime = (unsigned long)time(NULL);

	ptr = _tps_htable_val_buf;

	// base64 encode val values
	if(_tps_base64) {
		base64url_enc(md->x_vbranch1.s, md->x_vbranch1.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL, "%ld|%s", rectime,
				_tps_base64_buf[0]);
	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL, "%ld|%.*s", rectime,
				md->x_vbranch1.len, md->x_vbranch1.s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_VAL) {
		LM_ERR("failed to build htable value\n");
		return -1;
	}


	// insert key/val
	ret = helper_htable_insert(_tps_htable_transaction);
	if(ret < 0) {
		LM_ERR("failed to insert htable value\n");
		return -1;
	}


	// set expire for key/val
	expire = (unsigned long)_tps_api.get_branch_expire();
	if(expire == 0) {
		return 0;
	}

	ret = helper_htable_set_expire(_tps_htable_transaction, expire);
	if(ret < 0) {
		LM_ERR("failed to set expire\n");
		return -1;
	}

	return 0;
}

static int tps_htable_load_initial_method_branch(tps_data_t *md, tps_data_t *sd)
{
	str hkey;
	ht_cell_t *hval;
	char *ptr;
	int ret = 0;
	int i = 0;
	str xuuid = str_init("");
	str xtag = str_init("");
	str smethod = str_init("INVITE");

	// checks
	if(md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(md->a_callid.len <= 0 || md->b_tag.len <= 0) {
		LM_DBG("no call-id or to-tag for this message\n");
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	if(md->direction == TPS_DIR_DOWNSTREAM) {
		if(md->b_tag.s != NULL) {
			xtag.s = md->b_tag.s;
			xtag.len = md->b_tag.len;
		}
	} else {
		if(md->a_tag.s != NULL) {
			xtag.s = md->a_tag.s;
			xtag.len = md->a_tag.len;
		}
	}

	if(md->a_uuid.len > 1) {
		xuuid.s = md->a_uuid.s + 1;
		xuuid.len = md->a_uuid.len - 1;
	} else if(md->b_uuid.len > 1) {
		xuuid.s = md->b_uuid.s + 1;
		xuuid.len = md->b_uuid.len - 1;
	} else if(sd->a_uuid.len > 1) {
		xuuid.s = sd->a_uuid.s + 1;
		xuuid.len = sd->a_uuid.len - 1;
	} else if(sd->b_uuid.len > 1) {
		xuuid.s = sd->b_uuid.s + 1;
		xuuid.len = sd->b_uuid.len - 1;
	}

	if(md->s_method_id & (METHOD_SUBSCRIBE | METHOD_NOTIFY)) {
		smethod.s = "SUBSCRIBE";
		smethod.len = 9;
	}

	ptr = _tps_htable_key_buf;

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(md->s_method.s, md->s_method.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(md->a_callid.s, md->a_callid.len, _tps_base64_buf[1],
				TPS_BASE64_SIZE - 1);
		base64url_enc(md->b_tag.s, md->b_tag.len, _tps_base64_buf[2],
				TPS_BASE64_SIZE - 1);
		base64url_enc(
				xuuid.s, xuuid.len, _tps_base64_buf[3], TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s|%s|%s|x%s",
				_tps_base64_buf[0], _tps_base64_buf[1], _tps_base64_buf[2],
				_tps_base64_buf[3]);
	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s|%.*s|%.*s|x%.*s",
				smethod.len, smethod.s, md->a_callid.len, md->a_callid.s,
				xtag.len, xtag.s, xuuid.len, xuuid.s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}

	hkey.s = _tps_htable_key_buf;
	hkey.len = strlen(_tps_htable_key_buf);

	memset(sd, 0, sizeof(tps_data_t));
	sd->cp = sd->cbuf;

	// load hval from transaction htable module
	hval = _tps_htable_api.get_clone(&_tps_htable_transaction, &hkey);
	if(hval != NULL) {
		LM_DBG("hval = %.*s\n", hval->value.s.len, hval->value.s.s);
		i = 0;
		while((ptr = strsep(&hval->value.s.s, "|")) != NULL) {
			// base64 decode val values
			if(_tps_base64 && strlen(ptr) > 0 && i != 0 && i < 2) {
				base64url_dec(ptr, strlen(ptr), _tps_base64_buf[0],
						TPS_BASE64_SIZE - 1);
				ptr = _tps_base64_buf[0];
			}

			if(i == 0) {
				// skip rectime, not needed
				;
			} else if(i == 1) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_vbranch1);
			} else {
				// skip, not needed
				;
			}
			i++;
		}
		LM_DBG("AFTER LOAD initial method branch %.*s", sd->x_vbranch1.len,
				sd->x_vbranch1.s);
		pkg_free(hval);
	} else {
		LM_DBG("no initial branch found with key %.*s\n", hkey.len, hkey.s);
		return 1;
	}

	return 0;
}


/**
 *		TRANSACTION API FUNCTIONS
 */
int tps_htable_insert_branch(tps_data_t *td)
{
	unsigned long rectime = 0;
	int ret = 0, expire = 0;
	char *ptr;

	// checks
	if(td == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(td->x_vbranch1.len <= 0) {
		LM_DBG("no via branch for this message\n");
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	ptr = _tps_htable_key_buf;

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(td->x_vbranch1.s, td->x_vbranch1.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s", _tps_base64_buf[0]);
	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s", td->x_vbranch1.len,
				td->x_vbranch1.s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}


	// build val
	rectime = (unsigned long)time(NULL);
	ptr = _tps_htable_val_buf;

	// base64 encode val values
	if(_tps_base64) {
		base64url_enc(td->a_callid.s, td->a_callid.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_uuid.s, td->a_uuid.len, _tps_base64_buf[1],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_uuid.s, td->b_uuid.len, _tps_base64_buf[2],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_via.s, td->x_via.len, _tps_base64_buf[3],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_vbranch1.s, td->x_vbranch1.len, _tps_base64_buf[4],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_rr.s, td->x_rr.len, _tps_base64_buf[5],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->y_rr.s, td->y_rr.len, _tps_base64_buf[6],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_rr.s, td->s_rr.len, _tps_base64_buf[7],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_uri.s, td->x_uri.len, _tps_base64_buf[8],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_tag.s, td->x_tag.len, _tps_base64_buf[9],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_method.s, td->s_method.len, _tps_base64_buf[10],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_cseq.s, td->s_cseq.len, _tps_base64_buf[11],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_contact.s, td->a_contact.len, _tps_base64_buf[12],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_contact.s, td->b_contact.len, _tps_base64_buf[13],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->as_contact.s, td->as_contact.len, _tps_base64_buf[14],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->bs_contact.s, td->bs_contact.len, _tps_base64_buf[15],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_tag.s, td->a_tag.len, _tps_base64_buf[16],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_tag.s, td->b_tag.len, _tps_base64_buf[17],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_context.s, td->x_context.len, _tps_base64_buf[18],
				TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL,
				"%ld|%s|%s|%s|%d|%s|%s|%s|%s|%s|%s|%s|%s|"
				"%s|%s|%s|%s|%s|%s|%s|%s",
				rectime, _tps_base64_buf[0], _tps_base64_buf[1],
				_tps_base64_buf[2], td->direction, _tps_base64_buf[3],
				_tps_base64_buf[4], _tps_base64_buf[5], _tps_base64_buf[6],
				_tps_base64_buf[7], _tps_base64_buf[8], _tps_base64_buf[9],
				_tps_base64_buf[10], _tps_base64_buf[11], _tps_base64_buf[12],
				_tps_base64_buf[13], _tps_base64_buf[14], _tps_base64_buf[15],
				_tps_base64_buf[16], _tps_base64_buf[17], _tps_base64_buf[18]);
	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL,
				"%ld|%.*s|%.*s|%.*s|%d|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|"
				"%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s",
				rectime, td->a_callid.len, td->a_callid.s, td->a_uuid.len,
				td->a_uuid.s, td->b_uuid.len, td->b_uuid.s, td->direction,
				td->x_via.len, td->x_via.s, td->x_vbranch1.len,
				td->x_vbranch1.s, td->x_rr.len, td->x_rr.s, td->y_rr.len,
				td->y_rr.s, td->s_rr.len, td->s_rr.s, td->x_uri.len,
				td->x_uri.s, td->x_tag.len, td->x_tag.s, td->s_method.len,
				td->s_method.s, td->s_cseq.len, td->s_cseq.s, td->a_contact.len,
				td->a_contact.s, td->b_contact.len, td->b_contact.s,
				td->as_contact.len, td->as_contact.s, td->bs_contact.len,
				td->bs_contact.s, td->a_tag.len, td->a_tag.s, td->b_tag.len,
				td->b_tag.s, td->x_context.len, td->x_context.s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_VAL) {
		LM_ERR("failed to build htable val\n");
		return -1;
	}


	// insert key/val
	ret = helper_htable_insert(_tps_htable_transaction);
	if(ret < 0) {
		LM_ERR("failed to insert htable value\n");
		return -1;
	}


	// set expire for key/val
	expire = (unsigned long)_tps_api.get_branch_expire();
	if(expire == 0) {
		return 0;
	}

	ret = helper_htable_set_expire(_tps_htable_transaction, expire);
	if(ret < 0) {
		LM_ERR("failed to set expire\n");
		return -1;
	}

	return 0;
}


int tps_htable_load_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	str hkey;
	ht_cell_t *hval;
	char *ptr;
	int ret = 0;
	int i = 0;
	str *xvbranch1 = NULL;
	tps_data_t id;

	// checks
	if(msg == NULL || md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(mode == 0 && md->x_vbranch1.len <= 0) {
		LM_DBG("no via branch for this message\n");
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	if(mode == 0) {
		/* load same transaction using Via branch */
		xvbranch1 = &md->x_vbranch1;
	} else {
		/* load corresponding INVITE or SUBSCRIBE transaction using call-id + to-tag */
		if(tps_htable_load_initial_method_branch(md, &id) < 0) {
			LM_ERR("failed to load the %.*s branch value\n", md->s_method.len,
					md->s_method.s);
			return -1;
		}
		xvbranch1 = &id.x_vbranch1;
	}
	if(xvbranch1->len <= 0 || xvbranch1->s == NULL) {
		LM_DBG("branch value not found (mode %u)\n", mode);
		return 1;
	}

	ptr = _tps_htable_key_buf;

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(xvbranch1->s, xvbranch1->len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s", _tps_base64_buf[0]);
	} else {
		ret = snprintf(
				ptr, TPS_HTABLE_SIZE_KEY, "%.*s", xvbranch1->len, xvbranch1->s);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}

	hkey.s = _tps_htable_key_buf;
	hkey.len = strlen(_tps_htable_key_buf);

	memset(sd, 0, sizeof(tps_data_t));
	sd->cp = sd->cbuf;

	// load hval from transaction htable module
	hval = _tps_htable_api.get_clone(&_tps_htable_transaction, &hkey);
	if(hval != NULL) {
		LM_DBG("hval = %.*s\n", hval->value.s.len, hval->value.s.s);
		i = 0;
		while((ptr = strsep(&hval->value.s.s, "|")) != NULL) {
			// base64 decode val values
			if(_tps_base64 && strlen(ptr) > 0 && i != 0 && i != 4 && i < 21) {
				base64url_dec(ptr, strlen(ptr), _tps_base64_buf[0],
						TPS_BASE64_SIZE - 1);
				ptr = _tps_base64_buf[0];
			}

			if(i == 0) {
				// skip rectime, not needed
				;
			} else if(i == 1) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_callid);
			} else if(i == 2) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_uuid);
			} else if(i == 3) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_uuid);
			} else if(i == 4) {
				// skip direction, not needed
				;
			} else if(i == 5) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_via);
			} else if(i == 6) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_vbranch1);
			} else if(i == 7) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_rr);
			} else if(i == 8) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->y_rr);
			} else if(i == 9) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_rr);
			} else if(i == 10) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_uri);
			} else if(i == 11) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_tag);
			} else if(i == 12) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_method);
			} else if(i == 13) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_cseq);
			} else if(i == 14) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_contact);
			} else if(i == 15) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_contact);
			} else if(i == 16) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->as_contact);
			} else if(i == 17) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->bs_contact);
			} else if(i == 18) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_tag);
			} else if(i == 19) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_tag);
			} else if(i == 20) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_context);
			} else {
				// skip, not needed
				;
			}
			i++;
		}
		LM_DBG("AFTER LOAD branch "
			   "%.*s,%.*s,%.*s,%d,%.*s,%.*s,%.*s,%.*s,%.*s,%.*s,%.*s,%.*s,%.*s,"
			   "%.*s,%.*s,%.*s,%.*s,%.*s,%.*s,%.*s",
				sd->a_callid.len, sd->a_callid.s, sd->a_uuid.len, sd->a_uuid.s,
				sd->b_uuid.len, sd->b_uuid.s, sd->direction, sd->x_via.len,
				sd->x_via.s, sd->x_vbranch1.len, sd->x_vbranch1.s, sd->x_rr.len,
				sd->x_rr.s, sd->y_rr.len, sd->y_rr.s, sd->s_rr.len, sd->s_rr.s,
				sd->x_uri.len, sd->x_uri.s, sd->x_tag.len, sd->x_tag.s,
				sd->s_method.len, sd->s_method.s, sd->s_cseq.len, sd->s_cseq.s,
				sd->a_contact.len, sd->a_contact.s, sd->b_contact.len,
				sd->b_contact.s, sd->as_contact.len, sd->as_contact.s,
				sd->bs_contact.len, sd->bs_contact.s, sd->a_tag.len,
				sd->a_tag.s, sd->b_tag.len, sd->b_tag.s, sd->x_context.len,
				sd->x_context.s);
		pkg_free(hval);
	} else {
		LM_DBG("no branch found with key %.*s\n", hkey.len, hkey.s);
		return 1;
	}

	return 0;
}


int tps_htable_update_branch(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	int do_update = 0;
	tps_data_t hval;
	int ret = 0;

	// checks
	if(msg == NULL || md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(sd->a_uuid.len <= 0 && sd->b_uuid.len <= 0) {
		LM_DBG("no uuid for this message %d\n", sd->s_method_id);
		return -1;
	}

	LM_DBG("HERE\n");

	if(md->s_method_id == METHOD_INVITE
			|| md->s_method_id == METHOD_SUBSCRIBE) {
		if(tps_htable_insert_initial_method_branch(md, sd) < 0) {
			LM_ERR("failed to insert %.*s extra initial branch data\n",
					md->s_method.len, md->s_method.s);
			return -1;
		}
	}

	// load hval
	memset(&hval, 0, sizeof(tps_data_t));
	hval.cp = hval.cbuf;

	if(mode & TPS_DBU_CONTACT) {
		if(!do_update) {
			ret = tps_htable_load_branch(msg, sd, &hval, 0);
			if(ret != 0) {
				LM_ERR("branch not loaded\n");
				return -1;
			}
		}
		do_update = 1;

		if(md->a_contact.len > 0) {
			hval.a_contact = md->a_contact;
		}
		if(md->b_contact.len > 0) {
			hval.b_contact = md->b_contact;
		}
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type == SIP_REPLY) {
		if(msg->first_line.u.reply.statuscode >= 180
				&& msg->first_line.u.reply.statuscode < 200) {
			if(!do_update) {
				ret = tps_htable_load_branch(msg, sd, &hval, 0);
				if(ret != 0) {
					LM_ERR("branch not loaded\n");
					return -1;
				}
			}
			do_update = 1;

			if(md->b_rr.len > 0) {
				hval.b_rr = md->b_rr;
			}
			if(md->b_tag.len > 0) {
				hval.b_tag = md->b_tag;
			}
		}
	}

	if(!do_update) {
		return 0;
	}


	// insert hval
	ret = tps_htable_insert_branch(&hval);
	if(ret != 0) {
		LM_ERR("branch not inserted\n");
		return -1;
	}

	return 0;
}


int tps_htable_clean_branches(void)
{
	LM_DBG("HERE\n");
	return 0;
}


/**
 *		DIALOG API FUNCTIONS
 */

static int tps_htable_insert_dialog_helper(tps_data_t *td, int set_expire)
{
	char *ptr;
	int ret = 0;
	unsigned long rectime = 0;
	int expire = 0;

	// checks
	if(td == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(td->a_uuid.len <= 0 && td->b_uuid.len <= 0) {
		LM_DBG("no uuid for this message %d\n", td->s_method_id);
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	ptr = _tps_htable_key_buf;
	ret = (td->a_uuid.len > 0) ? snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
				  td->a_uuid.len, td->a_uuid.s)
							   : snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
									   td->b_uuid.len, td->b_uuid.s);
	if(ret < 0) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}
	ptr[0] = 'a';

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(_tps_htable_key_buf, strlen(_tps_htable_key_buf),
				_tps_base64_buf[0], TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s", _tps_base64_buf[0]);
	}


	// build val
	rectime = (unsigned long)time(NULL);
	ptr = _tps_htable_val_buf;

	// base64 encode val values
	if(_tps_base64) {
		base64url_enc(td->a_callid.s, td->a_callid.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_uuid.s, td->a_uuid.len, _tps_base64_buf[1],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_uuid.s, td->b_uuid.len, _tps_base64_buf[2],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_contact.s, td->a_contact.len, _tps_base64_buf[3],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_contact.s, td->b_contact.len, _tps_base64_buf[4],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->as_contact.s, td->as_contact.len, _tps_base64_buf[5],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->bs_contact.s, td->bs_contact.len, _tps_base64_buf[6],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_tag.s, td->a_tag.len, _tps_base64_buf[7],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_tag.s, td->b_tag.len, _tps_base64_buf[8],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_rr.s, td->a_rr.len, _tps_base64_buf[9],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_rr.s, td->b_rr.len, _tps_base64_buf[10],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_rr.s, td->s_rr.len, _tps_base64_buf[11],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_uri.s, td->a_uri.len, _tps_base64_buf[12],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_uri.s, td->b_uri.len, _tps_base64_buf[13],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->r_uri.s, td->r_uri.len, _tps_base64_buf[14],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->a_srcaddr.s, td->a_srcaddr.len, _tps_base64_buf[15],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->b_srcaddr.s, td->b_srcaddr.len, _tps_base64_buf[16],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_method.s, td->s_method.len, _tps_base64_buf[17],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->s_cseq.s, td->s_cseq.len, _tps_base64_buf[18],
				TPS_BASE64_SIZE - 1);
		base64url_enc(td->x_context.s, td->x_context.len, _tps_base64_buf[19],
				TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL,
				"%ld|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|"
				"%d|%s|%s|%s|%s|%s|%s|%s|%s",
				rectime, _tps_base64_buf[0], _tps_base64_buf[1],
				_tps_base64_buf[2], _tps_base64_buf[3], _tps_base64_buf[4],
				_tps_base64_buf[5], _tps_base64_buf[6], _tps_base64_buf[7],
				_tps_base64_buf[8], _tps_base64_buf[9], _tps_base64_buf[10],
				_tps_base64_buf[11], td->iflags, _tps_base64_buf[12],
				_tps_base64_buf[13], _tps_base64_buf[14], _tps_base64_buf[15],
				_tps_base64_buf[16], _tps_base64_buf[17], _tps_base64_buf[18],
				_tps_base64_buf[19]);
	} else {
		ret = snprintf(ptr, TPS_HTABLE_SIZE_VAL,
				"%ld|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*"
				"s|"
				"%d|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s|%.*s",
				rectime, td->a_callid.len, td->a_callid.s, td->a_uuid.len,
				td->a_uuid.s, td->b_uuid.len, td->b_uuid.s, td->a_contact.len,
				td->a_contact.s, td->b_contact.len, td->b_contact.s,
				td->as_contact.len, td->as_contact.s, td->bs_contact.len,
				td->bs_contact.s, td->a_tag.len, td->a_tag.s, td->b_tag.len,
				td->b_tag.s, td->a_rr.len, td->a_rr.s, td->b_rr.len, td->b_rr.s,
				td->s_rr.len, td->s_rr.s, td->iflags, td->a_uri.len,
				td->a_uri.s, td->b_uri.len, td->b_uri.s, td->r_uri.len,
				td->r_uri.s, td->a_srcaddr.len, td->a_srcaddr.s,
				td->b_srcaddr.len, td->b_srcaddr.s, td->s_method.len,
				td->s_method.s, td->s_cseq.len, td->s_cseq.s, td->x_context.len,
				td->x_context.s);
	}

	if(ret < 0) {
		LM_ERR("failed to build htable val\n");
		return -1;
	}


	// insert key/val
	ret = helper_htable_insert(_tps_htable_dialog);
	if(ret < 0) {
		LM_ERR("failed to insert into topos_dialog hastable\n");
		return -1;
	}


	// set expire for key/val
	if(td->s_method.len == 9 && strncmp(td->s_method.s, "SUBSCRIBE", 9) == 0) {
		expire = td->expires;
	} else {
		expire = _tps_api.get_dialog_expire();
	}

	if(expire == 0 || set_expire == 0) {
		return 0;
	}

	ret = helper_htable_set_expire(_tps_htable_dialog, expire);
	if(ret < 0) {
		LM_ERR("failed to set expire\n");
		return -1;
	}

	return 0;
}

int tps_htable_insert_dialog(tps_data_t *td)
{
	return tps_htable_insert_dialog_helper(td, 1);
}


/**
 *
 */
int tps_htable_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	char *ptr;
	int ret = 0;
	int i = 0;
	str hkey;
	ht_cell_t *hval;

	// checks
	if(msg == NULL || md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(md->a_uuid.len <= 0 && md->b_uuid.len <= 0) {
		LM_DBG("no dlg uuid provided\n");
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	ptr = _tps_htable_key_buf;
	ret = (md->a_uuid.len > 0) ? snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
				  md->a_uuid.len, md->a_uuid.s)
							   : snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
									   md->b_uuid.len, md->b_uuid.s);
	if(ret < 0) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}
	ptr[0] = 'a';

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(_tps_htable_key_buf, strlen(_tps_htable_key_buf),
				_tps_base64_buf[0], TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s", _tps_base64_buf[0]);
	}

	hkey.s = _tps_htable_key_buf;
	hkey.len = strlen(_tps_htable_key_buf);


	// get hval
	memset(sd, 0, sizeof(tps_data_t));
	sd->cp = sd->cbuf;

	hval = _tps_htable_api.get_clone(&_tps_htable_dialog, &hkey);
	if(hval != NULL) {
		LM_DBG("hval = %.*s\n", hval->value.s.len, hval->value.s.s);
		i = 0;
		while((ptr = strsep(&hval->value.s.s, "|")) != NULL) {
			// base64 decode val values
			if(_tps_base64 && strlen(ptr) > 0 && i != 0 && i != 13 && i < 22) {
				base64url_dec(ptr, strlen(ptr), _tps_base64_buf[0],
						TPS_BASE64_SIZE - 1);
				ptr = _tps_base64_buf[0];
			}

			if(i == 0) {
				// skip rectime, not needed
				;
			} else if(i == 1) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_callid);
			} else if(i == 2) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_uuid);
			} else if(i == 3) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_uuid);
			} else if(i == 4) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_contact);
			} else if(i == 5) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_contact);
			} else if(i == 6) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->as_contact);
			} else if(i == 7) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->bs_contact);
			} else if(i == 8) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_tag);
			} else if(i == 9) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_tag);
			} else if(i == 10) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_rr);
			} else if(i == 11) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_rr);
			} else if(i == 12) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_rr);
			} else if(i == 13) {
				// skip iflags, not needed
				;
			} else if(i == 14) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_uri);
			} else if(i == 15) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_uri);
			} else if(i == 16) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->r_uri);
			} else if(i == 17) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->a_srcaddr);
			} else if(i == 18) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->b_srcaddr);
			} else if(i == 19) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_method);
			} else if(i == 20) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->s_cseq);
			} else if(i == 21) {
				TPS_HTABLE_DATA_APPEND(sd, &hkey, ptr, &sd->x_context);
			} else {
				// skip, not needed
				;
			}
			i++;
		}
		pkg_free(hval);
	} else {
		LM_DBG("no dlg found with key %.*s\n", hkey.len, hkey.s);
		return 1;
	}

	return 0;
}

int tps_htable_update_dialog(
		sip_msg_t *msg, tps_data_t *md, tps_data_t *sd, uint32_t mode)
{
	int do_update = 0;
	int ret = 0;
	tps_data_t hval;
	int32_t liflags;

	// checks
	if(msg == NULL || md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if(sd->a_uuid.len <= 0 && sd->b_uuid.len <= 0) {
		LM_DBG("no uuid for this message %d\n", sd->s_method_id);
		return -1;
	}

	LM_DBG("HERE\n");

	// load hval
	memset(&hval, 0, sizeof(tps_data_t));
	hval.cp = hval.cbuf;

	if(mode & TPS_DBU_CONTACT) {
		if(!do_update) {
			ret = tps_htable_load_dialog(msg, sd, &hval);
			if(ret != 0) {
				LM_ERR("dialog not loaded\n");
				return -1;
			}
		}
		do_update = 1;

		if(md->a_contact.len > 0) {
			hval.a_contact = md->a_contact;
		}
		if(md->b_contact.len > 0) {
			hval.b_contact = md->b_contact;
		}
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type == SIP_REPLY) {
		if(sd->b_tag.len <= 0 && msg->first_line.u.reply.statuscode >= 200
				&& msg->first_line.u.reply.statuscode < 300) {
			if(!do_update) {
				ret = tps_htable_load_dialog(msg, sd, &hval);
				if(ret != 0) {
					LM_ERR("dialog not loaded\n");
					return -1;
				}
			}
			do_update = 1;


			if((sd->iflags & TPS_IFLAG_DLGON) == 0) {
				if(md->b_rr.len > 0) {
					hval.b_rr = md->b_rr;
				}
			}

			if(md->b_tag.len > 0) {
				hval.b_tag = md->b_tag;
			}
			liflags = sd->iflags | TPS_IFLAG_DLGON;
			hval.iflags = liflags;
		}
	}

	if(sd->b_tag.len > 0 && ((mode & TPS_DBU_BRR) || (mode & TPS_DBU_ARR))) {
		if(((md->direction == TPS_DIR_DOWNSTREAM)
				   && (msg->first_line.type == SIP_REPLY))
				|| ((md->direction == TPS_DIR_UPSTREAM)
						&& (msg->first_line.type == SIP_REQUEST))) {
			if(((sd->iflags & TPS_IFLAG_DLGON) == 0) && (mode & TPS_DBU_BRR)) {
				if(!do_update) {
					ret = tps_htable_load_dialog(msg, sd, &hval);
					if(ret != 0) {
						LM_ERR("dialog not loaded\n");
						return -1;
					}
				}
				do_update = 1;
				if(md->b_rr.len > 0) {
					hval.b_rr = md->b_rr;
				}
			}
		} else {
			if(((sd->iflags & TPS_IFLAG_DLGON) == 0) && (mode & TPS_DBU_ARR)) {
				if(!do_update) {
					ret = tps_htable_load_dialog(msg, sd, &hval);
					if(ret != 0) {
						LM_ERR("dialog not loaded\n");
						return -1;
					}
				}
				do_update = 1;
				if(md->a_rr.len > 0) {
					hval.a_rr = md->a_rr;
				}
				if(md->s_rr.len > 0) {
					hval.s_rr = md->s_rr;
				}
			}
		}
	}

	if(mode & TPS_DBU_TIME) {
		if(!do_update) {
			ret = tps_htable_load_dialog(msg, sd, &hval);
			if(ret != 0) {
				LM_ERR("dialog not loaded\n");
				return -1;
			}
		}
		do_update = 1;

		if(md->expires > 0) {
			hval.expires = md->expires;
		}
	}

	if(!do_update) {
		return 0;
	}


	// insert hval
	ret = tps_htable_insert_dialog_helper(&hval, 0);
	if(ret != 0) {
		LM_ERR("dialog not inserted\n");
		return -1;
	}

	return 0;
}

/**
 *
 */
int tps_htable_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	char *ptr;
	int ret = 0;
	int expire = 0;

	// checks
	if(msg == NULL || md == NULL || sd == NULL) {
		LM_DBG("NULL pointers");
		return -1;
	}

	if((md->s_method_id & METHOD_BYE)
			|| (msg->first_line.u.reply.statuscode > 299
					&& (get_cseq(msg)->method_id
							& (METHOD_INVITE | METHOD_SUBSCRIBE)))
			|| (md->s_method_id == METHOD_SUBSCRIBE && md->expires == 0)) {
		// all good, end dialog by setting htable expire
	} else {
		LM_DBG("no method for ending dialog %d\n", md->s_method_id);
		return 0;
	}

	if(sd->a_uuid.len <= 0 && sd->b_uuid.len <= 0) {
		LM_DBG("no uuid for this message %d\n", sd->s_method_id);
		return -1;
	}

	LM_DBG("HERE\n");

	// build key
	ptr = _tps_htable_key_buf;
	ret = (sd->a_uuid.len > 0) ? snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
				  sd->a_uuid.len, sd->a_uuid.s)
							   : snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%.*s",
									   sd->b_uuid.len, sd->b_uuid.s);
	if(ret < 0) {
		LM_ERR("failed to build htable key\n");
		return -1;
	}
	ptr[0] = 'a';

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(_tps_htable_key_buf, strlen(_tps_htable_key_buf),
				_tps_base64_buf[0], TPS_BASE64_SIZE - 1);
		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s", _tps_base64_buf[0]);
	}


	// dialog ended -- keep it for branch lifetime only
	expire = _tps_api.get_branch_expire();
	if(expire == 0) {
		return 0;
	}

	ret = helper_htable_set_expire(_tps_htable_dialog, expire);
	if(ret < 0) {
		LM_ERR("failed to set expire\n");
		return -1;
	}


	return 0;
}

/**
 *
 */
int tps_htable_clean_dialogs(void)
{
	LM_DBG("HERE\n");
	return 0;
}
