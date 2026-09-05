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

static str _tps_htable_xpfx = str_init("x");

/***		HTABLE RECORD FORMAT FUNCTIONS		***/

/**
 * The htable records are built by joining the field values with a separator,
 * they are parsed back by position, therefore a field value containing the
 * separator (e.g., taken from Call-ID or another header) has to be escaped,
 * otherwise it shifts all the following fields of the record.
 * - the escape character escapes the separator as well as itself
 * - values without separator or escape character are stored unchanged, keeping
 *   the format compatible with the records built by previous versions
 */
#define TPS_HTABLE_FSEP '|'
#define TPS_HTABLE_FESC '\\'

typedef struct tps_htable_rec
{
	char *buf;
	int size;
	int len;
} tps_htable_rec_t;

#define TPS_REC_ADD(_rec, _v)                    \
	do {                                         \
		if(tps_htable_rec_add((_rec), (_v)) < 0) \
			return -1;                           \
	} while(0)

#define TPS_REC_CAT(_rec, _v)                    \
	do {                                         \
		if(tps_htable_rec_cat((_rec), (_v)) < 0) \
			return -1;                           \
	} while(0)

#define TPS_REC_ADDI(_rec, _v)                    \
	do {                                          \
		if(tps_htable_rec_addi((_rec), (_v)) < 0) \
			return -1;                            \
	} while(0)

/**
 * Initialize a record builder over the buffer buf of size bytes
 */
static void tps_htable_rec_init(tps_htable_rec_t *rec, char *buf, int size)
{
	rec->buf = buf;
	rec->size = size;
	rec->len = 0;
	rec->buf[0] = '\0';
}

/**
 * Append the escaped value of sv, without a field separator
 * - returns 0 on success, -1 if the value does not fit in the buffer
 */
static int tps_htable_rec_cat(tps_htable_rec_t *rec, str *sv)
{
	int i;

	if(sv == NULL || sv->s == NULL || sv->len <= 0) {
		return 0;
	}

	for(i = 0; i < sv->len; i++) {
		if(sv->s[i] == TPS_HTABLE_FSEP || sv->s[i] == TPS_HTABLE_FESC) {
			if(rec->len + 2 >= rec->size) {
				return -1;
			}
			rec->buf[rec->len++] = TPS_HTABLE_FESC;
		} else {
			if(rec->len + 1 >= rec->size) {
				return -1;
			}
		}
		rec->buf[rec->len++] = sv->s[i];
	}
	rec->buf[rec->len] = '\0';

	return 0;
}

/**
 * Append the field separator, unless it is the first field of the record
 * - returns 0 on success, -1 if it does not fit in the buffer
 */
static int tps_htable_rec_sep(tps_htable_rec_t *rec)
{
	if(rec->len == 0) {
		return 0;
	}
	if(rec->len + 1 >= rec->size) {
		return -1;
	}
	rec->buf[rec->len++] = TPS_HTABLE_FSEP;
	rec->buf[rec->len] = '\0';

	return 0;
}

/**
 * Append a new field with the escaped value of sv
 * - returns 0 on success, -1 if it does not fit in the buffer
 */
static int tps_htable_rec_add(tps_htable_rec_t *rec, str *sv)
{
	if(tps_htable_rec_sep(rec) < 0) {
		return -1;
	}

	return tps_htable_rec_cat(rec, sv);
}

/**
 * Append a new field with the value of the integer v
 * - returns 0 on success, -1 if it does not fit in the buffer
 */
static int tps_htable_rec_addi(tps_htable_rec_t *rec, long v)
{
	int ret = 0;

	if(tps_htable_rec_sep(rec) < 0) {
		return -1;
	}

	ret = snprintf(rec->buf + rec->len, rec->size - rec->len, "%ld", v);
	if(ret < 0 || ret >= rec->size - rec->len) {
		return -1;
	}
	rec->len += ret;

	return 0;
}

/**
 * Get the next field of the record pointed by sptr, like strsep() does for the
 * field separator, but skipping the escaped separators
 * - the returned field value is unescaped in place, it can only get shorter
 */
static char *tps_htable_rec_next(char **sptr)
{
	char *val;
	char *r;
	char *w;

	if(sptr == NULL || *sptr == NULL) {
		return NULL;
	}

	val = *sptr;
	for(r = val; *r != '\0'; r++) {
		if(*r == TPS_HTABLE_FESC && *(r + 1) != '\0') {
			r++;
			continue;
		}
		if(*r == TPS_HTABLE_FSEP) {
			break;
		}
	}

	if(*r == TPS_HTABLE_FSEP) {
		*r = '\0';
		*sptr = r + 1;
	} else {
		*sptr = NULL;
	}

	for(r = w = val; *r != '\0'; r++) {
		if(*r == TPS_HTABLE_FESC && *(r + 1) != '\0') {
			r++;
		}
		*w++ = *r;
	}
	*w = '\0';

	return val;
}

/**
 * Build the key of an initial method branch record in the global key buffer
 * - returns the length of the key, -1 if it does not fit in the buffer
 */
static int tps_htable_key_build_imb(str *callid, str *tag, str *xuuid)
{
	tps_htable_rec_t rec;

	tps_htable_rec_init(&rec, _tps_htable_key_buf, TPS_HTABLE_SIZE_KEY);

	TPS_REC_ADD(&rec, callid);
	TPS_REC_ADD(&rec, tag);
	TPS_REC_ADD(&rec, &_tps_htable_xpfx);
	TPS_REC_CAT(&rec, xuuid);

	return rec.len;
}

/**
 * Build the value of an initial method branch record in the global value buffer
 * - returns the length of the value, -1 if it does not fit in the buffer
 */
static int tps_htable_val_build_imb(unsigned long rectime, tps_data_t *md)
{
	tps_htable_rec_t rec;

	tps_htable_rec_init(&rec, _tps_htable_val_buf, TPS_HTABLE_SIZE_VAL);

	TPS_REC_ADDI(&rec, (long)rectime);
	TPS_REC_ADD(&rec, &md->x_vbranch1);

	return rec.len;
}

/**
 * Build the value of a branch record in the global value buffer
 * - returns the length of the value, -1 if it does not fit in the buffer
 */
static int tps_htable_val_build_branch(unsigned long rectime, tps_data_t *td)
{
	tps_htable_rec_t rec;

	tps_htable_rec_init(&rec, _tps_htable_val_buf, TPS_HTABLE_SIZE_VAL);

	TPS_REC_ADDI(&rec, (long)rectime);
	TPS_REC_ADD(&rec, &td->a_callid);
	TPS_REC_ADD(&rec, &td->a_uuid);
	TPS_REC_ADD(&rec, &td->b_uuid);
	TPS_REC_ADDI(&rec, (long)td->direction);
	TPS_REC_ADD(&rec, &td->x_via);
	TPS_REC_ADD(&rec, &td->x_vbranch1);
	TPS_REC_ADD(&rec, &td->x_rr);
	TPS_REC_ADD(&rec, &td->y_rr);
	TPS_REC_ADD(&rec, &td->s_rr);
	TPS_REC_ADD(&rec, &td->x_uri);
	TPS_REC_ADD(&rec, &td->x_tag);
	TPS_REC_ADD(&rec, &td->s_method);
	TPS_REC_ADD(&rec, &td->s_cseq);
	TPS_REC_ADD(&rec, &td->a_contact);
	TPS_REC_ADD(&rec, &td->b_contact);
	TPS_REC_ADD(&rec, &td->as_contact);
	TPS_REC_ADD(&rec, &td->bs_contact);
	TPS_REC_ADD(&rec, &td->a_tag);
	TPS_REC_ADD(&rec, &td->b_tag);
	TPS_REC_ADD(&rec, &td->x_context);

	return rec.len;
}

/**
 * Build the value of a dialog record in the global value buffer
 * - returns the length of the value, -1 if it does not fit in the buffer
 */
static int tps_htable_val_build_dialog(unsigned long rectime, tps_data_t *td)
{
	tps_htable_rec_t rec;

	tps_htable_rec_init(&rec, _tps_htable_val_buf, TPS_HTABLE_SIZE_VAL);

	TPS_REC_ADDI(&rec, (long)rectime);
	TPS_REC_ADD(&rec, &td->a_callid);
	TPS_REC_ADD(&rec, &td->a_uuid);
	TPS_REC_ADD(&rec, &td->b_uuid);
	TPS_REC_ADD(&rec, &td->a_contact);
	TPS_REC_ADD(&rec, &td->b_contact);
	TPS_REC_ADD(&rec, &td->as_contact);
	TPS_REC_ADD(&rec, &td->bs_contact);
	TPS_REC_ADD(&rec, &td->a_tag);
	TPS_REC_ADD(&rec, &td->b_tag);
	TPS_REC_ADD(&rec, &td->a_rr);
	TPS_REC_ADD(&rec, &td->b_rr);
	TPS_REC_ADD(&rec, &td->s_rr);
	TPS_REC_ADDI(&rec, (long)td->iflags);
	TPS_REC_ADD(&rec, &td->a_uri);
	TPS_REC_ADD(&rec, &td->b_uri);
	TPS_REC_ADD(&rec, &td->r_uri);
	TPS_REC_ADD(&rec, &td->a_srcaddr);
	TPS_REC_ADD(&rec, &td->b_srcaddr);
	TPS_REC_ADD(&rec, &td->s_method);
	TPS_REC_ADD(&rec, &td->s_cseq);
	TPS_REC_ADD(&rec, &td->x_context);

	return rec.len;
}

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
		base64url_enc(md->a_callid.s, md->a_callid.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(md->b_tag.s, md->b_tag.len, _tps_base64_buf[1],
				TPS_BASE64_SIZE - 1);
		base64url_enc(
				xuuid.s, xuuid.len, _tps_base64_buf[2], TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s|%s|x%s",
				_tps_base64_buf[0], _tps_base64_buf[1], _tps_base64_buf[2]);

	} else {
		ret = tps_htable_key_build_imb(&md->a_callid, &md->b_tag, &xuuid);
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
		ret = tps_htable_val_build_imb(rectime, md);
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

static int tps_htable_load_initial_method_branch(tps_data_t *md, tps_data_t *sd)
{
	str hkey;
	ht_cell_t *hval;
	char *ptr;
	int ret = 0;
	int i = 0;
	str xuuid = str_init("");
	str xtag = str_init("");

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

	ptr = _tps_htable_key_buf;

	// base64 encode key values
	if(_tps_base64) {
		base64url_enc(md->a_callid.s, md->a_callid.len, _tps_base64_buf[0],
				TPS_BASE64_SIZE - 1);
		base64url_enc(
				xtag.s, xtag.len, _tps_base64_buf[1], TPS_BASE64_SIZE - 1);
		base64url_enc(
				xuuid.s, xuuid.len, _tps_base64_buf[2], TPS_BASE64_SIZE - 1);

		ret = snprintf(ptr, TPS_HTABLE_SIZE_KEY, "%s|%s|x%s",
				_tps_base64_buf[0], _tps_base64_buf[1], _tps_base64_buf[2]);
	} else {
		ret = tps_htable_key_build_imb(&md->a_callid, &xtag, &xuuid);
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
		while((ptr = tps_htable_rec_next(&hval->value.s.s)) != NULL) {
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
		ret = tps_htable_val_build_branch(rectime, td);
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
		while((ptr = tps_htable_rec_next(&hval->value.s.s)) != NULL) {
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
				hval.y_rr = md->b_rr;
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
	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
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
		ret = tps_htable_val_build_dialog(rectime, td);
	}

	if(ret < 0 || ret >= TPS_HTABLE_SIZE_VAL) {
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
	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
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
	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
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
		while((ptr = tps_htable_rec_next(&hval->value.s.s)) != NULL) {
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

	if((mode & TPS_DBU_CONTACT)
			&& (md->a_contact.len > 0 || md->b_contact.len > 0)) {
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

	/* persist {a,b}s_contact */
	if((mode & TPS_DBU_SCONTACT)
			&& (md->as_contact.len > 0 || md->bs_contact.len > 0)) {
		if(!do_update) {
			ret = tps_htable_load_dialog(msg, sd, &hval);
			if(ret != 0) {
				LM_ERR("dialog not loaded\n");
				return -1;
			}
		}
		do_update = 1;

		if(md->as_contact.len > 0) {
			hval.as_contact = md->as_contact;
		}
		if(md->bs_contact.len > 0) {
			hval.bs_contact = md->bs_contact;
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

	/* persist s_rr */
	if((mode & TPS_DBU_SRR) && md->s_rr.len > 0) {
		if(!do_update) {
			ret = tps_htable_load_dialog(msg, sd, &hval);
			if(ret != 0) {
				LM_ERR("dialog not loaded\n");
				return -1;
			}
		}
		do_update = 1;
		hval.s_rr = md->s_rr;
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
	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
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
	if(ret < 0 || ret >= TPS_HTABLE_SIZE_KEY) {
		LM_ERR("failed to build htable key\n");
		return -1;
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
