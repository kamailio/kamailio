/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * UAC Kamailio-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC Kamailio-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief Kamailio uac :: header replacement/retrieval functions
 * \ingroup uac
 * Module: \ref uac
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>

#include "../../core/parser/parse_from.h"
#include "../../core/mem/mem.h"
#include "../../core/data_lump.h"
#include "../../core/route.h"
#include "../../core/basex.h"
#include "../../modules/tm/h_table.h"
#include "../../modules/tm/tm_load.h"
#include "../rr/api.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"
#include "../htable/ht_api.h"
#include "../htable/api.h"


#include "replace.h"
#include "uac_cseq.h"

extern str uac_passwd;
extern int restore_mode;
extern str rr_from_param;
extern str rr_to_param;
extern struct tm_binds uac_tmb;
extern struct rr_binds uac_rrb;

extern str restore_from_avp;
extern str restore_to_avp;
extern avp_flags_t restore_from_avp_type;
extern int_str restore_from_avp_name;
extern avp_flags_t restore_to_avp_type;
extern int_str restore_to_avp_name;
extern str uac_restore_htable;
extern int uac_restore_htable_initexpire;
extern int uac_restore_htable_rmexpire;
extern int uac_auth_cseq_tracking;
extern htable_api_t uac_htable_api;

struct dlg_binds dlg_api;
static str from_dlgvar[] = {str_init("_uac_fu"), str_init("_uac_funew"),
		str_init("_uac_fdp"), str_init("_uac_fdpnew")};
static str to_dlgvar[] = {str_init("_uac_to"), str_init("_uac_tonew"),
		str_init("_uac_tdp"), str_init("_uac_tdpnew")};

static char enc_table64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
							"abcdefghijklmnopqrstuvwxyz0123456789+/";

static int dec_table64[256];

static void restore_uris_reply(struct cell *t, int type, struct tmcb_params *p);

static void replace_callback(
		struct dlg_cell *dlg, int type, struct dlg_cb_params *_params);
static inline struct lump *get_display_anchor(struct sip_msg *msg,
		struct hdr_field *hdr, struct to_body *body, str *dsp);

#define text3B64_len(_l) ((((_l) + 2) / 3) << 2)

#define UAC_HTABLE_VALUE_OVERHEAD 3

typedef struct uac_htable_values
{
	str old_uri;
	str new_uri;
	str old_display;
	str new_display;
	ht_cell_t *cell;
	char *decoded;
} uac_htable_values_t;

static int uac_htable_call_key(struct sip_msg *msg, char type, str *key)
{
	if(parse_headers(msg, HDR_CALLID_F, 0) < 0 || msg->callid == NULL
			|| msg->callid->body.s == NULL || msg->callid->body.len <= 0) {
		LM_ERR("failed to parse Call-ID header for htable key\n");
		return -1;
	}
	if(msg->callid->body.len > INT_MAX - 2) {
		LM_ERR("htable call key is too long\n");
		return -1;
	}
	key->len = msg->callid->body.len + 2;
	key->s = pkg_malloc(key->len);
	if(key->s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	key->s[0] = type;
	key->s[1] = ':';
	memcpy(key->s + 2, msg->callid->body.s, msg->callid->body.len);
	return 0;
}

static inline int uac_htable_origin_key(struct sip_msg *msg, str *key)
{
	return uac_htable_call_key(msg, 'o', key);
}

static inline int uac_htable_active_key(struct sip_msg *msg, str *key)
{
	return uac_htable_call_key(msg, 'a', key);
}

static int uac_htable_tag_key(
		struct sip_msg *msg, char type, str *tag, str *key)
{
	char *p;
	int n;

	if(parse_headers(msg, HDR_CALLID_F, 0) < 0 || msg->callid == NULL
			|| msg->callid->body.s == NULL || msg->callid->body.len <= 0) {
		LM_ERR("failed to parse Call-ID header for htable key\n");
		return -1;
	}
	if(tag == NULL || tag->s == NULL || tag->len <= 0) {
		LM_ERR("missing original From tag for htable key\n");
		return -1;
	}
	if(msg->callid->body.len > INT_MAX - 16
			|| tag->len > INT_MAX - msg->callid->body.len - 16) {
		LM_ERR("htable key is too long\n");
		return -1;
	}

	key->len = msg->callid->body.len + tag->len + 16;
	key->s = pkg_malloc(key->len);
	if(key->s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	p = key->s;
	n = snprintf(p, key->len - (int)(p - key->s), "%c:%d:", type,
			msg->callid->body.len);
	if(n < 0 || n >= key->len - (int)(p - key->s)) {
		LM_ERR("failed to format htable key\n");
		pkg_free(key->s);
		key->s = NULL;
		key->len = 0;
		return -1;
	}
	p += n;
	memcpy(p, msg->callid->body.s, msg->callid->body.len);
	p += msg->callid->body.len;
	*p++ = ':';
	memcpy(p, tag->s, tag->len);
	p += tag->len;
	key->len = (int)(p - key->s);
	return 0;
}

static inline int uac_htable_key(
		struct sip_msg *msg, int check_from, str *tag, str *key)
{
	return uac_htable_tag_key(msg, check_from ? 'f' : 't', tag, key);
}

static inline int uac_htable_cseq_key(struct sip_msg *msg, str *tag, str *key)
{
	return uac_htable_tag_key(msg, 'c', tag, key);
}

static int uac_htable_store_origin(struct sip_msg *msg)
{
	str key = STR_NULL;
	struct to_body *from;
	numstr_ut value;
	numstr_ut expire;

	if(parse_from_header(msg) < 0) {
		LM_ERR("failed to parse From header for htable origin\n");
		return -1;
	}
	from = get_from(msg);
	if(from->tag_value.s == NULL || from->tag_value.len <= 0) {
		LM_ERR("missing original From tag for htable origin\n");
		return -1;
	}
	if(uac_htable_origin_key(msg, &key) < 0)
		return -1;
	value.s = from->tag_value;
	if(uac_htable_api.set(&uac_restore_htable, &key, AVP_VAL_STR, &value, 1)
			< 0) {
		LM_ERR("failed to store original From tag in htable\n");
		pkg_free(key.s);
		return -1;
	}
	expire.n = uac_restore_htable_initexpire;
	if(uac_htable_api.set_expire(&uac_restore_htable, &key, 0, &expire) < 0) {
		LM_ERR("failed to set htable origin expiration\n");
		uac_htable_api.rm(&uac_restore_htable, &key);
		pkg_free(key.s);
		return -1;
	}
	pkg_free(key.s);
	return 0;
}

static int uac_htable_direction_from_tag(
		struct sip_msg *msg, str *tag, int *upstream)
{
	str from_tag;
	str to_tag;

	if(parse_from_header(msg) < 0) {
		LM_ERR("failed to parse From header for htable direction\n");
		return -1;
	}
	if(msg->to == NULL
			&& (parse_headers(msg, HDR_TO_F, 0) != 0 || msg->to == NULL)) {
		LM_ERR("failed to parse To header for htable direction\n");
		return -1;
	}
	from_tag = get_from(msg)->tag_value;
	to_tag = get_to(msg)->tag_value;
	if(from_tag.len == tag->len && from_tag.s != NULL
			&& memcmp(from_tag.s, tag->s, tag->len) == 0) {
		*upstream = 0;
		return 0;
	}
	if(to_tag.len == tag->len && to_tag.s != NULL
			&& memcmp(to_tag.s, tag->s, tag->len) == 0) {
		*upstream = 1;
		return 0;
	}
	LM_ERR("original From tag does not match current From or To tag\n");
	return -1;
}

static int uac_htable_unpack(ht_cell_t *cell, uac_htable_values_t *values)
{
	str *items[4];
	str encoded[4];
	unsigned int decoded_size;
	char *end;
	char *p;
	char *q;
	int len;
	int i;

	if(cell == NULL || !(cell->flags & AVP_VAL_STR) || cell->value.s.s == NULL
			|| cell->value.s.len < UAC_HTABLE_VALUE_OVERHEAD) {
		return -1;
	}
	p = cell->value.s.s;
	end = p + cell->value.s.len;
	decoded_size = 4;
	for(i = 0; i < 4; i++) {
		encoded[i].s = p;
		if(i < 3) {
			q = memchr(p, '|', end - p);
			if(q == NULL) {
				LM_ERR("invalid uac htable value fields\n");
				return -1;
			}
			encoded[i].len = (int)(q - p);
			p = q + 1;
		} else {
			encoded[i].len = (int)(end - p);
			p = end;
		}
		if((encoded[i].len & 3) != 0 || encoded[i].len > (INT_MAX >> 2)) {
			LM_ERR("invalid uac htable encoded field length\n");
			return -1;
		}
		len = base64_max_dec_len(encoded[i].len);
		if(len < 0 || (unsigned int)len > UINT_MAX - decoded_size) {
			LM_ERR("invalid uac htable encoded field length\n");
			return -1;
		}
		decoded_size += (unsigned int)len;
	}
	if(decoded_size > INT_MAX) {
		LM_ERR("decoded uac htable value is too large\n");
		return -1;
	}
	values->decoded = pkg_malloc(decoded_size);
	if(values->decoded == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	items[0] = &values->old_uri;
	items[1] = &values->new_uri;
	items[2] = &values->old_display;
	items[3] = &values->new_display;
	p = values->decoded;
	for(i = 0; i < 4; i++) {
		items[i]->s = p;
		if(encoded[i].len == 0) {
			items[i]->len = 0;
		} else {
			items[i]->len = base64url_dec(encoded[i].s, encoded[i].len, p,
					(int)(values->decoded + decoded_size - p));
			if(items[i]->len < 0) {
				LM_ERR("failed to decode uac htable value field\n");
				pkg_free(values->decoded);
				values->decoded = NULL;
				return -1;
			}
		}
		p += items[i]->len;
		*p++ = '\0';
	}
	values->cell = cell;
	return 0;
}

static void uac_htable_values_free(uac_htable_values_t *values)
{
	if(values->decoded != NULL) {
		pkg_free(values->decoded);
		values->decoded = NULL;
	}
	if(values->cell != NULL) {
		pkg_free(values->cell);
		values->cell = NULL;
	}
}

static int uac_htable_is_active(struct sip_msg *msg, int refresh)
{
	str key = STR_NULL;
	ht_cell_t *cell;
	int ret = 0;

	if(uac_htable_active_key(msg, &key) < 0)
		return -1;
	cell = uac_htable_api.get_clone(&uac_restore_htable, &key);
	if(cell == NULL)
		goto done;
	if((cell->flags & AVP_VAL_STR) || cell->value.n != 1) {
		LM_ERR("invalid uac htable active marker\n");
		ret = -1;
		goto done;
	}
	ret = 1;
	if(refresh
			&& uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0) {
		LM_WARN("failed to refresh uac htable active marker expiration\n");
	}

done:
	if(cell != NULL)
		pkg_free(cell);
	pkg_free(key.s);
	return ret;
}

static int uac_htable_load_origin(
		struct sip_msg *msg, int refresh, ht_cell_t **origin)
{
	str key = STR_NULL;
	int active;

	*origin = NULL;
	if(uac_htable_origin_key(msg, &key) < 0)
		return -1;
	*origin = uac_htable_api.get_clone(&uac_restore_htable, &key);
	if(*origin != NULL && refresh) {
		active = uac_htable_is_active(msg, 1);
		if(active > 0
				&& uac_htable_api.refresh_expire(&uac_restore_htable, &key)
						   < 0) {
			LM_WARN("failed to refresh uac htable origin expiration\n");
		}
	}
	pkg_free(key.s);
	if(*origin == NULL)
		return 1;
	if(!((*origin)->flags & AVP_VAL_STR) || (*origin)->value.s.s == NULL
			|| (*origin)->value.s.len <= 0) {
		LM_ERR("invalid original From tag in uac htable\n");
		pkg_free(*origin);
		*origin = NULL;
		return -1;
	}
	return 0;
}

static int uac_htable_direction(struct sip_msg *msg, int refresh, int *upstream)
{
	ht_cell_t *origin;
	int ret;

	ret = uac_htable_load_origin(msg, refresh, &origin);
	if(ret != 0)
		return ret;
	ret = uac_htable_direction_from_tag(msg, &origin->value.s, upstream);
	pkg_free(origin);
	return ret;
}

int uac_htable_cseq_update(
		struct sip_msg *msg, unsigned int *diff, int *upstream)
{
	str key = STR_NULL;
	ht_cell_t *origin = NULL;
	int active;
	int value;
	int ret;
	int_str expire;

	if(diff == NULL || upstream == NULL)
		return -1;
	*diff = 0;
	*upstream = 0;

	ret = uac_htable_load_origin(msg, 0, &origin);
	if(ret == 1) {
		if(uac_htable_store_origin(msg) < 0)
			return -1;
		ret = uac_htable_load_origin(msg, 0, &origin);
	}
	if(ret != 0)
		return -1;
	if(uac_htable_direction_from_tag(msg, &origin->value.s, upstream) < 0)
		goto error;
	if(*upstream) {
		pkg_free(origin);
		return 0;
	}
	if(uac_htable_cseq_key(msg, &origin->value.s, &key) < 0)
		goto error;
	active = uac_htable_is_active(msg, 0);
	if(active < 0)
		goto error;
	if(uac_htable_api.add_ival(&uac_restore_htable, &key, 1, 0, &value) < 0
			|| value <= 0) {
		LM_ERR("failed to increment uac htable cseq difference\n");
		goto error;
	}
	if(active > 0) {
		if(uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0)
			LM_WARN("failed to refresh uac htable cseq expiration\n");
	} else {
		expire.n = uac_restore_htable_initexpire;
		if(uac_htable_api.set_expire(&uac_restore_htable, &key, 0, &expire)
				< 0) {
			LM_WARN("failed to set uac htable cseq expiration\n");
		}
	}
	*diff = (unsigned int)value;
	pkg_free(key.s);
	pkg_free(origin);
	return 0;

error:
	if(key.s != NULL)
		pkg_free(key.s);
	if(origin != NULL)
		pkg_free(origin);
	return -1;
}

int uac_htable_cseq_get(
		struct sip_msg *msg, int refresh, unsigned int *diff, int *upstream)
{
	str key = STR_NULL;
	ht_cell_t *origin = NULL;
	ht_cell_t *cell = NULL;
	int active;
	int ret;

	if(diff == NULL || upstream == NULL)
		return -1;
	*diff = 0;
	*upstream = 0;
	ret = uac_htable_load_origin(msg, refresh, &origin);
	if(ret != 0)
		return ret;
	if(uac_htable_direction_from_tag(msg, &origin->value.s, upstream) < 0) {
		ret = -1;
		goto done;
	}
	if(uac_htable_cseq_key(msg, &origin->value.s, &key) < 0) {
		ret = -1;
		goto done;
	}
	cell = uac_htable_api.get_clone(&uac_restore_htable, &key);
	if(cell == NULL) {
		ret = 1;
		goto done;
	}
	if((cell->flags & AVP_VAL_STR) || cell->value.n <= 0
			|| cell->value.n > UINT_MAX) {
		LM_ERR("invalid uac htable cseq difference\n");
		ret = -1;
		goto done;
	}
	*diff = (unsigned int)cell->value.n;
	ret = 0;
	if(refresh) {
		active = uac_htable_is_active(msg, 0);
		if(active > 0
				&& uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0)
			LM_WARN("failed to refresh uac htable cseq expiration\n");
	}

done:
	if(cell != NULL)
		pkg_free(cell);
	if(key.s != NULL)
		pkg_free(key.s);
	if(origin != NULL)
		pkg_free(origin);
	return ret;
}

static int uac_htable_refresh_optional(str *key, const char *what)
{
	ht_cell_t *cell;

	cell = uac_htable_api.get_clone(&uac_restore_htable, key);
	if(cell == NULL)
		return 0;
	pkg_free(cell);
	if(uac_htable_api.refresh_expire(&uac_restore_htable, key) < 0) {
		LM_WARN("failed to refresh uac htable %s expiration\n", what);
		return -1;
	}
	return 0;
}

static int uac_htable_expire_optional(
		str *key, int_str *expire, const char *what)
{
	ht_cell_t *cell;

	cell = uac_htable_api.get_clone(&uac_restore_htable, key);
	if(cell == NULL)
		return 0;
	pkg_free(cell);
	if(uac_htable_api.set_expire(&uac_restore_htable, key, 0, expire) < 0) {
		LM_WARN("failed to shorten uac htable %s expiration\n", what);
		return -1;
	}
	return 0;
}

static int uac_htable_refresh_call(struct sip_msg *msg)
{
	str key = STR_NULL;
	ht_cell_t *origin;
	int check_from;
	int ret = 0;

	if(uac_htable_load_origin(msg, 0, &origin) != 0)
		return -1;
	for(check_from = 0; check_from <= 1; check_from++) {
		if(uac_htable_key(msg, check_from, &origin->value.s, &key) < 0) {
			ret = -1;
			continue;
		}
		if(uac_htable_refresh_optional(&key, "replacement") < 0) {
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_cseq_key(msg, &origin->value.s, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_refresh_optional(&key, "cseq") < 0) {
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_origin_key(msg, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0) {
			LM_WARN("failed to refresh uac htable origin expiration\n");
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_active_key(msg, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0) {
			LM_WARN("failed to refresh uac htable active marker expiration\n");
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	pkg_free(origin);
	return ret;
}

static int uac_htable_activate_call(struct sip_msg *msg)
{
	str key = STR_NULL;
	numstr_ut value;

	if(uac_htable_active_key(msg, &key) < 0)
		return -1;
	value.n = 1;
	if(uac_htable_api.set(&uac_restore_htable, &key, 0, &value, 1) < 0) {
		LM_ERR("failed to store uac htable active marker\n");
		pkg_free(key.s);
		return -1;
	}
	pkg_free(key.s);
	return uac_htable_refresh_call(msg);
}

static int uac_htable_set_call_expire(struct sip_msg *msg, int seconds)
{
	str key = STR_NULL;
	ht_cell_t *origin;
	int_str expire;
	int check_from;
	int ret = 0;

	if(uac_htable_load_origin(msg, 0, &origin) != 0)
		return -1;
	expire.n = seconds;
	for(check_from = 0; check_from <= 1; check_from++) {
		if(uac_htable_key(msg, check_from, &origin->value.s, &key) < 0) {
			ret = -1;
			continue;
		}
		if(uac_htable_expire_optional(&key, &expire, "replacement") < 0) {
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_cseq_key(msg, &origin->value.s, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_expire_optional(&key, &expire, "cseq") < 0) {
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_origin_key(msg, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_api.set_expire(&uac_restore_htable, &key, 0, &expire)
				< 0) {
			LM_WARN("failed to shorten uac htable origin expiration\n");
			ret = -1;
		}
		pkg_free(key.s);
		key.s = NULL;
		key.len = 0;
	}
	if(uac_htable_active_key(msg, &key) < 0) {
		ret = -1;
	} else {
		if(uac_htable_api.set_expire(&uac_restore_htable, &key, 0, &expire)
				< 0) {
			LM_WARN("failed to shorten uac htable active marker expiration\n");
			ret = -1;
		}
		pkg_free(key.s);
	}
	pkg_free(origin);
	return ret;
}

static int uac_htable_load(struct sip_msg *msg, int check_from, int refresh,
		int *upstream, uac_htable_values_t *values)
{
	str key = STR_NULL;
	ht_cell_t *origin;
	ht_cell_t *cell;
	int ret;

	memset(values, 0, sizeof(*values));
	ret = uac_htable_load_origin(msg, refresh, &origin);
	if(ret != 0)
		return ret;
	if(upstream != NULL
			&& uac_htable_direction_from_tag(msg, &origin->value.s, upstream)
					   < 0) {
		pkg_free(origin);
		return -1;
	}
	if(uac_htable_key(msg, check_from, &origin->value.s, &key) < 0) {
		pkg_free(origin);
		return -1;
	}
	cell = uac_htable_api.get_clone(&uac_restore_htable, &key);
	if(cell != NULL && refresh && uac_htable_is_active(msg, 0) > 0) {
		if(uac_htable_api.refresh_expire(&uac_restore_htable, &key) < 0) {
			LM_WARN("failed to refresh uac htable entry expiration\n");
		}
	}
	pkg_free(key.s);
	pkg_free(origin);
	if(cell == NULL)
		return 1;
	if(uac_htable_unpack(cell, values) < 0) {
		pkg_free(cell);
		return -1;
	}
	return 0;
}

static int uac_htable_store(struct sip_msg *msg, int check_from,
		struct to_body *body, str *display, str *uri)
{
	uac_htable_values_t stored;
	str key = STR_NULL;
	ht_cell_t *origin = NULL;
	str old_uri;
	str old_display;
	str new_display = STR_NULL;
	str packed = STR_NULL;
	str *items[4];
	numstr_ut value;
	numstr_ut expire;
	unsigned int total;
	char *p;
	int len;
	int i;
	int ret;

	if(uac_htable_store_origin(msg) < 0)
		return -1;
	ret = uac_htable_load(msg, check_from, 0, NULL, &stored);
	if(ret < 0)
		return -1;
	if(ret == 0) {
		old_uri = stored.old_uri;
		old_display = stored.old_display;
	} else {
		old_uri = body->uri;
		old_display = body->display;
	}
	if(display != NULL)
		new_display = *display;
	else
		new_display = old_display;

	items[0] = &old_uri;
	items[1] = uri;
	items[2] = &old_display;
	items[3] = &new_display;
	total = UAC_HTABLE_VALUE_OVERHEAD;
	for(i = 0; i < 4; i++) {
		if(items[i] == NULL || items[i]->len < 0
				|| items[i]->len > (INT_MAX >> 2)) {
			LM_ERR("invalid value for uac htable storage\n");
			uac_htable_values_free(&stored);
			return -1;
		}
		len = base64_enc_len(items[i]->len);
		if(len < 0 || (unsigned int)len > UINT_MAX - total) {
			LM_ERR("encoded uac htable value is too large\n");
			uac_htable_values_free(&stored);
			return -1;
		}
		total += (unsigned int)len;
	}
	if(total >= INT_MAX) {
		LM_ERR("uac htable value is too large\n");
		uac_htable_values_free(&stored);
		return -1;
	}
	packed.s = pkg_malloc(total + 1);
	if(packed.s == NULL) {
		PKG_MEM_ERROR;
		uac_htable_values_free(&stored);
		return -1;
	}
	packed.len = (int)total;
	p = packed.s;
	for(i = 0; i < 4; i++) {
		len = base64url_enc(
				items[i]->s, items[i]->len, p, (int)(packed.s + total + 1 - p));
		if(len < 0) {
			LM_ERR("failed to encode uac htable value field\n");
			pkg_free(packed.s);
			uac_htable_values_free(&stored);
			return -1;
		}
		p += len;
		if(i < 3)
			*p++ = '|';
	}
	*p = '\0';

	if(uac_htable_load_origin(msg, 0, &origin) != 0)
		goto error;
	if(uac_htable_key(msg, check_from, &origin->value.s, &key) < 0)
		goto error;
	pkg_free(origin);
	origin = NULL;
	value.s = packed;
	if(uac_htable_api.set(&uac_restore_htable, &key, AVP_VAL_STR, &value, 1)
			< 0) {
		LM_ERR("failed to store uac replacement in htable\n");
		goto error;
	}
	expire.n = uac_restore_htable_initexpire;
	if(uac_htable_api.set_expire(&uac_restore_htable, &key, 0, &expire) < 0) {
		LM_ERR("failed to set uac htable entry expiration\n");
		uac_htable_api.rm(&uac_restore_htable, &key);
		goto error;
	}

	pkg_free(key.s);
	pkg_free(packed.s);
	uac_htable_values_free(&stored);
	return 0;

error:
	if(origin != NULL)
		pkg_free(origin);
	if(key.s != NULL)
		pkg_free(key.s);
	pkg_free(packed.s);
	uac_htable_values_free(&stored);
	return -1;
}

static int uac_replace_header_value(struct sip_msg *msg, struct hdr_field *hdr,
		struct to_body *body, str *new_uri, str *new_display)
{
	struct lump *l = NULL;
	str buf;
	char *p;

	if(new_uri == NULL || new_uri->s == NULL || new_uri->len <= 0
			|| new_uri->len > MAX_URI_SIZE || new_display == NULL
			|| new_display->len < 0 || new_display->len > INT_MAX - 2) {
		LM_ERR("invalid stored replacement value\n");
		return -1;
	}

	if(body->display.s != NULL && body->display.len > 0) {
		l = del_lump(msg, body->display.s - msg->buf, body->display.len, 0);
		if(l == NULL) {
			LM_ERR("display del lump failed\n");
			return -1;
		}
	}
	if(new_display->len > 0) {
		buf.s = pkg_malloc(new_display->len + 2);
		if(buf.s == NULL) {
			PKG_MEM_ERROR;
			return -1;
		}
		memcpy(buf.s, new_display->s, new_display->len);
		buf.len = new_display->len;
		if(l == NULL
				&& (l = get_display_anchor(msg, hdr, body, &buf)) == NULL) {
			LM_ERR("failed to insert display anchor\n");
			pkg_free(buf.s);
			return -1;
		}
		if(insert_new_lump_after(l, buf.s, buf.len, 0) == NULL) {
			LM_ERR("insert new display lump failed\n");
			pkg_free(buf.s);
			return -1;
		}
	}

	p = pkg_malloc(new_uri->len);
	if(p == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(p, new_uri->s, new_uri->len);
	l = del_lump(msg, body->uri.s - msg->buf, body->uri.len, 0);
	if(l == NULL) {
		LM_ERR("uri del lump failed\n");
		pkg_free(p);
		return -1;
	}
	if(insert_new_lump_after(l, p, new_uri->len, 0) == NULL) {
		LM_ERR("insert new uri lump failed\n");
		pkg_free(p);
		return -1;
	}
	return 0;
}

static int uac_htable_restore_uri(
		struct sip_msg *msg, str *restore_avp, int check_from)
{
	uac_htable_values_t values;
	struct hdr_field *hdr;
	struct to_body *body;
	str *new_uri;
	str *new_display;
	char *p;
	char *end;
	int i;
	int upstream;
	int target_to;
	numstr_ut avp_value;
	msg_flags_t flag;

	if(uac_htable_load(msg, check_from, 1, &upstream, &values) != 0) {
		LM_DBG("uac replacement not found in htable\n");
		return -1;
	}

	target_to = (check_from && upstream) || (!check_from && !upstream);
	new_uri = upstream ? &values.old_uri : &values.new_uri;
	new_display = upstream ? &values.old_display : &values.new_display;

	if(target_to) {
		if(msg->to == NULL
				&& (parse_headers(msg, HDR_TO_F, 0) != 0 || msg->to == NULL)) {
			LM_ERR("failed to parse To header\n");
			goto error;
		}
		hdr = msg->to;
		flag = FL_USE_UAC_TO;
	} else {
		if(parse_from_header(msg) < 0) {
			LM_ERR("failed to parse From header\n");
			goto error;
		}
		hdr = msg->from;
		flag = FL_USE_UAC_FROM;
	}
	body = (struct to_body *)hdr->parsed;

	if(restore_avp->s != NULL) {
		p = body->uri.s + body->uri.len;
		end = body->body.s + body->body.len;
		for(i = 0; p + i < end && isspace((unsigned char)p[i]); i++)
			;
		avp_value.s.len =
				p - body->body.s + ((p + i < end && p[i] == '>') ? (i + 1) : 0);
		avp_value.s.s = body->body.s;
		if(flag == FL_USE_UAC_FROM)
			add_avp(restore_from_avp_type, restore_from_avp_name, avp_value);
		else
			add_avp(restore_to_avp_type, restore_to_avp_name, avp_value);
	}

	if(uac_replace_header_value(msg, hdr, body, new_uri, new_display) < 0)
		goto error;
	msg->msg_flags |= flag;
	uac_htable_values_free(&values);
	return 0;

error:
	uac_htable_values_free(&values);
	return -1;
}

static int uac_htable_restore_reply(struct sip_msg *req, struct sip_msg *rpl,
		int check_from, unsigned int direction)
{
	uac_htable_values_t values;
	struct hdr_field *hdr;
	struct to_body *body;
	str *new_uri;
	str *new_display;
	int upstream;
	int target_to;
	int refresh;

	upstream = (direction == DLG_DIR_UPSTREAM);
	refresh = (req->first_line.u.request.method_value != METHOD_BYE);
	if(uac_htable_load(req, check_from, refresh, NULL, &values) != 0) {
		LM_ERR("uac reply replacement not found in htable\n");
		return -1;
	}

	target_to = (check_from && upstream) || (!check_from && !upstream);
	new_uri = upstream ? &values.new_uri : &values.old_uri;
	new_display = upstream ? &values.new_display : &values.old_display;
	if(target_to) {
		if(rpl->to == NULL
				&& (parse_headers(rpl, HDR_TO_F, 0) != 0 || rpl->to == NULL)) {
			LM_ERR("failed to parse To header in reply\n");
			goto error;
		}
		hdr = rpl->to;
	} else {
		if(parse_from_header(rpl) < 0) {
			LM_ERR("failed to parse From header in reply\n");
			goto error;
		}
		hdr = rpl->from;
	}
	body = (struct to_body *)hdr->parsed;
	if(uac_replace_header_value(rpl, hdr, body, new_uri, new_display) < 0)
		goto error;

	uac_htable_values_free(&values);
	return 0;

error:
	uac_htable_values_free(&values);
	return -1;
}

/* The reply, were the From-Line was replaced. */

void init_from_replacer(void)
{
	int i;

	for(i = 0; i < 256; i++)
		dec_table64[i] = -1;
	for(i = 0; i < 64; i++)
		dec_table64[(unsigned char)enc_table64[i]] = i;
}


static inline int encode_uri(str *src, str *dst)
{
	static char buf[text3B64_len(MAX_URI_SIZE)];
	int idx;
	int left;
	int block;
	int i, r;
	char *p;

	dst->len = text3B64_len(src->len);
	dst->s = buf;
	if(dst->len > text3B64_len(MAX_URI_SIZE)) {
		LM_ERR("uri too long\n");
		return -1;
	}

	for(idx = 0, p = buf; idx < src->len; idx += 3) {
		left = src->len - idx - 1;
		left = (left > 1 ? 2 : left);

		/* Collect 1 to 3 bytes to encode */
		block = 0;
		for(i = 0, r = 16; i <= left; i++, r -= 8) {
			block += ((unsigned char)src->s[idx + i]) << r;
		}

		/* Encode into 2-4 chars appending '=' if not enough data left.*/
		*(p++) = enc_table64[(block >> 18) & 0x3f];
		*(p++) = enc_table64[(block >> 12) & 0x3f];
		*(p++) = left > 0 ? enc_table64[(block >> 6) & 0x3f] : '-';
		*(p++) = left > 1 ? enc_table64[block & 0x3f] : '-';
	}

	return 0;
}


static inline int decode_uri(str *src, str *dst)
{
	static char buf[MAX_URI_SIZE];
	int block;
	int n;
	int idx;
	int end;
	int i, j;
	signed char c;

	/* sanity checks */
	if(!src) {
		LM_ERR("NULL src\n");
		return -1;
	}

	if(!dst) {
		LM_ERR("NULL dst\n");
		return -1;
	}

	if(!src->s || src->len == 0) {
		LM_ERR("empty src\n");
		return -1;
	}

	/* Count '-' at end and disregard them */
	for(n = 0, i = src->len - 1; src->s[i] == '-'; i--)
		n++;

	dst->len = ((src->len * 6) >> 3) - n;
	dst->s = buf;
	if(dst->len > MAX_URI_SIZE) {
		LM_ERR("uri too long\n");
		return -1;
	}

	end = src->len - n;
	for(i = 0, idx = 0; i < end; idx += 3) {
		/* Assemble three bytes into an int from four "valid" characters */
		block = 0;
		for(j = 0; j < 4 && i < end; j++) {
			c = dec_table64[(unsigned char)src->s[i++]];
			if(c < 0) {
				LM_ERR("invalid base64 string\"%.*s\"\n", src->len, src->s);
				return -1;
			}
			block += c << (18 - 6 * j);
		}

		/* Add the bytes */
		for(j = 0, n = 16; j < 3 && idx + j < dst->len; j++, n -= 8)
			buf[idx + j] = (char)((block >> n) & 0xff);
	}

	return 0;
}


static inline struct lump *get_display_anchor(struct sip_msg *msg,
		struct hdr_field *hdr, struct to_body *body, str *dsp)
{
	struct lump *l;
	char *p1;
	char *p2;

	/* is URI quoted or not? */
	p1 = hdr->name.s + hdr->name.len;
	for(p2 = body->uri.s - 1; p2 >= p1 && *p2 != '<'; p2--)
		;

	if(*p2 == '<') {
		/* is quoted */
		l = anchor_lump(msg, p2 - msg->buf, 0, 0);
		if(l == 0) {
			LM_ERR("unable to build lump anchor\n");
			return 0;
		}
		dsp->s[dsp->len++] = ' ';
		return l;
	}

	/* not quoted - more complicated....must place the closing bracket */
	l = anchor_lump(msg, (body->uri.s + body->uri.len) - msg->buf, 0, 0);
	if(l == 0) {
		LM_ERR("unable to build lump anchor\n");
		return 0;
	}
	p1 = (char *)pkg_malloc(1);
	if(p1 == 0) {
		PKG_MEM_ERROR;
		return 0;
	}
	*p1 = '>';
	if(insert_new_lump_after(l, p1, 1, 0) == 0) {
		LM_ERR("insert lump failed\n");
		pkg_free(p1);
		return 0;
	}
	/* build anchor for display */
	l = anchor_lump(msg, body->uri.s - msg->buf, 0, 0);
	if(l == 0) {
		LM_ERR("unable to build lump anchor\n");
		return 0;
	}
	dsp->s[dsp->len++] = ' ';
	dsp->s[dsp->len++] = '<';
	return l;
}


/*
 * replace uri and/or display name in FROM / TO header
 */
int replace_uri(struct sip_msg *msg, str *display, str *uri,
		struct hdr_field *hdr, str *rr_param, str *restore_avp, int check_from)
{
	static char buf_s[MAX_URI_SIZE];
	struct to_body *body;
	struct lump *l;
	struct cell *Trans;
	str replace;
	char *p;
	str luri;
	str param;
	str buf;
	msg_flags_t uac_flag;
	int i, del_offset, del_len;
	int_str avp_value;
	struct dlg_cell *dlg = 0;
	str *dlgvar_names;
	str display_tmp;
	str undefined_display = str_init("");

	uac_flag = (hdr == msg->from) ? FL_USE_UAC_FROM : FL_USE_UAC_TO;
	if(get_route_type() == REQUEST_ROUTE) {
		if(msg->msg_flags & uac_flag) {
			LM_ERR("called uac_replace_%s() multiple times on the message\n",
					(hdr == msg->from) ? "from" : "to");
			return -1;
		}
	}

	/* consistency check! in AUTO mode, do NOT allow URI changing
	 * in sequential request */
	if(restore_mode == UAC_AUTO_RESTORE && uri && uri->len) {
		if(msg->to == 0
				&& (parse_headers(msg, HDR_TO_F, 0) != 0 || msg->to == 0)) {
			LM_ERR("failed to parse TO hdr\n");
			goto error;
		}
		if(get_to(msg)->tag_value.len != 0) {
			LM_ERR("decline FROM replacing in sequential request in auto mode "
				   "(has TO tag)\n");
			goto error;
		}
	}

	body = (struct to_body *)hdr->parsed;

	if(restore_avp->s) {
		/* backup data in avp (if avp is set) */
		/* catch whitespace characters after uri */
		for(p = body->uri.s + body->uri.len, i = 0; isspace(p[i]); i++)
			;
		/* if ">" present after uri (and whitespace), catch it */
		avp_value.s.len = p - body->body.s + ((p[i] == '>') ? (i + 1) : 0);
		avp_value.s.s = body->body.s;

		LM_DBG("value to store is is '%.*s' and len is '%d'\n", avp_value.s.len,
				avp_value.s.s, avp_value.s.len);

		if(check_from) {
			LM_DBG("Storing in FROM-AVP (for use in reply): '%.*s' with len "
				   "'%d'\n",
					avp_value.s.len, avp_value.s.s, avp_value.s.len);
			add_avp(restore_from_avp_type, restore_from_avp_name, avp_value);
		} else {
			LM_DBG("Storing in TO-AVP (for use in reply): '%.*s' with len "
				   "'%d'\n",
					avp_value.s.len, avp_value.s.s, avp_value.s.len);
			add_avp(restore_to_avp_type, restore_to_avp_name, avp_value);
		}
	}

	/* first deal with display name */
	if(display) {
		/* must be replaced/ removed */
		l = 0;
		/* first remove the existing display */
		if(body->display.len) {
			del_offset = body->display.s - msg->buf;
			del_len = body->display.len;

			LM_DBG("removing display [%.*s]\n", body->display.len,
					body->display.s);

			/* if removing display, also remove trailing spaces after it */
			if(!display->len) {
				p = body->display.s + body->display.len;
				while(p < msg->buf + msg->len && *p == ' ') {
					del_len++;
					p++;
				}
			}

			/* build del lump */
			l = del_lump(msg, del_offset, del_len, 0);
			if(l == 0) {
				LM_ERR("display del lump failed\n");
				goto error;
			}
		}
		/* some new display to set? */
		if(display->len) {
			LM_DBG("adding new display [%.*s]\n", display->len, display->s);
			/* add the new display exactly over the deleted one */
			buf.s = pkg_malloc(display->len + 2);
			if(buf.s == 0) {
				PKG_MEM_ERROR;
				goto error;
			}
			memcpy(buf.s, display->s, display->len);
			buf.len = display->len;
			if(l == 0 && (l = get_display_anchor(msg, hdr, body, &buf)) == 0) {
				LM_ERR("failed to insert anchor\n");
				pkg_free(buf.s);
				goto error;
			}
			if(insert_new_lump_after(l, buf.s, buf.len, 0) == 0) {
				LM_ERR("insert new display lump failed\n");
				pkg_free(buf.s);
				goto error;
			}
		}
	}

	/* now handle the URI */
	if(uri == 0 || uri->len == 0)
		/* do not touch URI part */
		return 0;

	LM_DBG("uri to replace [%.*s]\n", body->uri.len, body->uri.s);
	LM_DBG("replacement uri is [%.*s]\n", uri->len, uri->s);

	/* build del/add lumps */
	if((l = del_lump(msg, body->uri.s - msg->buf, body->uri.len, 0)) == 0) {
		LM_ERR("del lump failed\n");
		goto error;
	}
	luri.len = uri->len;
	if(!(body->style & TBS_URI_ENCLOSED)) {
		/* existing uri not enclosed - check if new one has parameters */
		for(p = uri->s + uri->len - 1; p > uri->s; p--) {
			if(*p == ';') {
				luri.len += 2;
				break;
			}
		}
	}
	luri.s = pkg_malloc(luri.len);
	if(luri.s == 0) {
		PKG_MEM_ERROR;
		goto error;
	}
	if(luri.len == uri->len + 2) {
		luri.s[0] = '<';
		memcpy(luri.s + 1, uri->s, uri->len);
		luri.s[luri.len - 1] = '>';
	} else {
		memcpy(luri.s, uri->s, uri->len);
	}
	if(insert_new_lump_after(l, luri.s, luri.len, 0) == 0) {
		LM_ERR("insert new lump failed\n");
		pkg_free(luri.s);
		goto error;
	}

	if(restore_mode == UAC_NO_RESTORE)
		return 0;

	if(uac_restore_htable.len > 0) {
		if(uac_htable_store(msg, check_from, body, display, uri) < 0)
			goto error;
		goto restore_stored;
	}

	/* trying to get dialog */
	if(dlg_api.get_dlg) {
		dlg = dlg_api.get_dlg(msg);
	}

	if(dlg) {
		dlgvar_names = (uac_flag == FL_USE_UAC_FROM) ? from_dlgvar : to_dlgvar;
		if(dlg_api.get_dlg_varstatus(dlg, &dlgvar_names[0])) {

			LM_INFO("Already called uac_replace for this dialog\n");
			/* delete the from_new dlg var */

			if(dlg_api.set_dlg_var(dlg, &dlgvar_names[1], 0) < 0) {
				LM_ERR("cannot store new uri value\n");
				dlg_api.release_dlg(dlg);
				goto error;
			}
			LM_INFO("Deleted <%.*s> var in dialog\n", dlgvar_names[1].len,
					dlgvar_names[1].s);
		} else {
			/* the first time uac_replace is called for this dialog */
			/* store old URI value */
			if(dlg_api.set_dlg_var(dlg, &dlgvar_names[0], &body->uri) < 0) {
				LM_ERR("cannot store value\n");
				dlg_api.release_dlg(dlg);
				goto error;
			}
			LM_DBG("Stored <%.*s> var in dialog with value %.*s\n",
					dlgvar_names[0].len, dlgvar_names[0].s, body->uri.len,
					body->uri.s);

			if(dlg_api.register_dlgcb(dlg,
					   DLGCB_REQ_WITHIN | DLGCB_CONFIRMED | DLGCB_TERMINATED,
					   (void *)(unsigned long)replace_callback,
					   (void *)(unsigned long)uac_flag, 0)
					!= 0) {
				LM_ERR("cannot register callback\n");
				dlg_api.release_dlg(dlg);
				goto error;
			}
		}
		/* store new URI value */
		if(dlg_api.set_dlg_var(dlg, &dlgvar_names[1], uri) < 0) {
			LM_ERR("cannot store new uri value\n");
			dlg_api.release_dlg(dlg);
			goto error;
		}
		LM_DBG("Stored <%.*s> var in dialog with value %.*s\n",
				dlgvar_names[1].len, dlgvar_names[1].s, uri->len, uri->s);

		/* store the display name as well */
		if(body->display.s && body->display.len > 0) {
			display_tmp = body->display;
		} else {
			display_tmp = undefined_display;
		}
		if(dlg_api.set_dlg_var(dlg, &dlgvar_names[2], &display_tmp) < 0) {
			LM_ERR("cannot store display value\n");
			dlg_api.release_dlg(dlg);
			goto error;
		}
		LM_DBG("Stored <%.*s> var in dialog with value %.*s\n",
				dlgvar_names[1].len, dlgvar_names[2].s, display_tmp.len,
				display_tmp.s);

		if(display && display->s && display->len > 0) {
			display_tmp.s = display->s;
			display_tmp.len = display->len;
		} else {
			display_tmp = undefined_display;
		}
		if(dlg_api.set_dlg_var(dlg, &dlgvar_names[3], &display_tmp) < 0) {
			LM_ERR("cannot store new display value\n");
			dlg_api.release_dlg(dlg);
			goto error;
		}
		LM_DBG("Stored <%.*s> var in dialog with value %.*s\n",
				dlgvar_names[1].len, dlgvar_names[3].s, display_tmp.len,
				display_tmp.s);

		dlg_api.release_dlg(dlg);
	} else {
		if(!uac_rrb.append_fromtag) {
			LM_ERR("'append_fromtag' RR param is not enabled!"
				   " - required by AUTO restore mode\n");
			goto error;
		}

		/* build RR parameter */
		buf.s = buf_s;
		if(body->uri.len > uri->len) {
			if(body->uri.len > MAX_URI_SIZE) {
				LM_ERR("old %.*s uri too long\n", hdr->name.len, hdr->name.s);
				goto error;
			}
			memcpy(buf.s, body->uri.s, body->uri.len);
			for(i = 0; i < uri->len; i++)
				buf.s[i] ^= uri->s[i];
			buf.len = body->uri.len;
		} else {
			if(uri->len > MAX_URI_SIZE) {
				LM_ERR("new %.*s uri too long\n", hdr->name.len, hdr->name.s);
				goto error;
			}
			memcpy(buf.s, uri->s, uri->len);
			for(i = 0; i < body->uri.len; i++)
				buf.s[i] ^= body->uri.s[i];
			buf.len = uri->len;
		}

		/* encrypt parameter ;) */
		if(uac_passwd.len)
			for(i = 0; i < buf.len; i++)
				buf.s[i] ^= uac_passwd.s[i % uac_passwd.len];

		/* encode the param */
		if(encode_uri(&buf, &replace) < 0) {
			LM_ERR("failed to encode uris\n");
			goto error;
		}
		LM_DBG("encode is=<%.*s> len=%d\n", replace.len, replace.s,
				replace.len);

		/* add RR parameter */
		param.len = 1 + rr_param->len + 1 + replace.len;
		param.s = (char *)pkg_malloc(param.len);
		if(param.s == 0) {
			PKG_MEM_ERROR;
			goto error;
		}
		p = param.s;
		*(p++) = ';';
		memcpy(p, rr_param->s, rr_param->len);
		p += rr_param->len;
		*(p++) = '=';
		memcpy(p, replace.s, replace.len);

		if(uac_rrb.add_rr_param(msg, &param) != 0) {
			LM_ERR("add_RR_param failed\n");
			goto error1;
		}
		pkg_free(param.s);
	}

restore_stored:
	if((msg->msg_flags & (FL_USE_UAC_FROM | FL_USE_UAC_TO)) == 0) {
		/* add TM callback to restore the FROM/TO hdr in reply */
		if(uac_tmb.register_tmcb(
				   msg, 0, TMCB_RESPONSE_IN, restore_uris_reply, 0, 0)
				!= 1) {
			LM_ERR("failed to install TM callback\n");
			goto error;
		}
	}
	msg->msg_flags |= uac_flag;

	if((Trans = uac_tmb.t_gett()) != NULL && Trans != T_UNDEFINED
			&& Trans->uas.request) {
		Trans->uas.request->msg_flags |= uac_flag;
	}

	return 0;
error1:
	pkg_free(param.s);
error:
	return -1;
}


/*
 * return  0 - restored
 *        -1 - not restored or error
 */
int restore_uri(
		struct sip_msg *msg, str *rr_param, str *restore_avp, int check_from)
{
	struct lump *l;
	str param_val;
	str add_to_rr = {0, 0};
	struct to_body *old_body;
	str old_uri = {0, 0};
	str new_uri = {0, 0};
	char *p;
	int i;
	int_str avp_value;
	msg_flags_t flag;
	int bsize;

	if(uac_restore_htable.len > 0)
		return uac_htable_restore_uri(msg, restore_avp, check_from);

	/* we should process only sequential request, but since we are looking
	 * for Route param, the test is not really required -bogdan */

	LM_DBG("getting '%.*s' Route param\n", rr_param->len, rr_param->s);
	/* is there something to restore ? */
	if(uac_rrb.get_route_param(msg, rr_param, &param_val) != 0) {
		LM_DBG("route param '%.*s' not found\n", rr_param->len, rr_param->s);
		goto failed;
	}
	LM_DBG("route param is '%.*s' (len=%d)\n", param_val.len, param_val.s,
			param_val.len);

	/* decode the parameter val to a URI */
	if(decode_uri(&param_val, &new_uri) < 0) {
		LM_ERR("failed to decode uri\n");
		goto failed;
	}

	bsize = 3 + rr_param->len + param_val.len;
	add_to_rr.s = pkg_malloc(bsize);
	if(add_to_rr.s == 0) {
		add_to_rr.len = 0;
		PKG_MEM_ERROR;
		goto failed;
	}
	add_to_rr.len = snprintf(add_to_rr.s, bsize, ";%.*s=%.*s", rr_param->len,
			rr_param->s, param_val.len, param_val.s);

	if(add_to_rr.len < 0 || add_to_rr.len >= bsize) {
		LM_ERR("printing rr param failed\n");
		goto failed;
	}
	if(uac_rrb.add_rr_param(msg, &add_to_rr) != 0) {
		LM_ERR("add rr param failed\n");
		goto failed;
	}
	pkg_free(add_to_rr.s);
	add_to_rr.s = NULL;

	/* decrypt parameter */
	if(uac_passwd.len) {
		for(i = 0; i < new_uri.len; i++)
			new_uri.s[i] ^= uac_passwd.s[i % uac_passwd.len];
	}

	/* check the request direction */
	if((check_from && uac_rrb.is_direction(msg, RR_FLOW_UPSTREAM) == 0)
			|| (!check_from
					&& uac_rrb.is_direction(msg, RR_FLOW_DOWNSTREAM) == 0)) {
		/* replace the TO URI */
		if(msg->to == 0
				&& (parse_headers(msg, HDR_TO_F, 0) != 0 || msg->to == 0)) {
			LM_ERR("failed to parse TO hdr\n");
			goto failed;
		}
		old_body = (struct to_body *)msg->to->parsed;
		flag = FL_USE_UAC_TO;
		LM_DBG("replacing in To header\n");
	} else {
		/* replace the FROM URI */
		if(parse_from_header(msg) < 0) {
			LM_ERR("failed to find/parse FROM hdr\n");
			goto failed;
		}
		old_body = (struct to_body *)msg->from->parsed;
		flag = FL_USE_UAC_FROM;
		LM_DBG("replacing in From header\n");
	}

	if(restore_avp->s) {
		/* backup data to avp (if avp is set) */

		/* catch whitespace characters after uri */
		for(p = old_body->uri.s + old_body->uri.len, i = 0; isspace(p[i]); i++)
			;
		/* if ">" present after uri (and whitespace), catch it */
		avp_value.s.len = p - old_body->body.s + ((p[i] == '>') ? (i + 1) : 0);
		avp_value.s.s = old_body->body.s;

		if(flag == FL_USE_UAC_FROM) {
			LM_DBG("Storing in FROM-AVP (for use in reply): '%.*s' with len "
				   "'%d'\n",
					avp_value.s.len, avp_value.s.s, avp_value.s.len);
			add_avp(restore_from_avp_type, restore_from_avp_name, avp_value);
		} else {
			LM_DBG("Storing in TO-AVP (for use in reply): '%.*s' with len "
				   "'%d'\n",
					avp_value.s.len, avp_value.s.s, avp_value.s.len);
			add_avp(restore_to_avp_type, restore_to_avp_name, avp_value);
		}
	}

	old_uri = old_body->uri;

	/* get new uri */
	if(new_uri.len < old_uri.len) {
		LM_ERR("new URI [%.*s] shorter than old URI [%.*s]\n", new_uri.len,
				new_uri.s, old_uri.len, old_uri.s);
		goto failed;
	}
	for(i = 0; i < old_uri.len; i++) {
		new_uri.s[i] ^= old_uri.s[i];
		if(new_uri.s[i] == 0) {
			new_uri.len = i;
			break;
		}
	}
	if(new_uri.len == 0) {
		LM_ERR("new URI got 0 len\n");
		goto failed;
	}

	/* check if new uri has valid characters */
	for(i = 0; i < new_uri.len; i++) {
		if(!isprint(new_uri.s[i])) {
			LM_WARN("invalid char found in the new uri at pos %d (%c) [%.*s]\n",
					i, new_uri.s[i], new_uri.len, new_uri.s);
			LM_WARN("this can happen when URI values are altered by end points"
					" - skipping the update\n");
			goto failed;
		}
	}
	LM_DBG("decoded uris are: new=[%.*s] old=[%.*s]\n", new_uri.len, new_uri.s,
			old_uri.len, old_uri.s);

	/* duplicate the decoded value */
	p = pkg_malloc(new_uri.len);
	if(p == 0) {
		PKG_MEM_ERROR;
		goto failed;
	}
	memcpy(p, new_uri.s, new_uri.len);
	new_uri.s = p;

	/* build del/add lumps */
	l = del_lump(msg, old_uri.s - msg->buf, old_uri.len, 0);
	if(l == 0) {
		LM_ERR("del lump failed\n");
		goto failed1;
	}

	if(insert_new_lump_after(l, new_uri.s, new_uri.len, 0) == 0) {
		LM_ERR("insert new lump failed\n");
		goto failed1;
	}

	msg->msg_flags |= flag;

	return 0;
failed1:
	pkg_free(new_uri.s);
failed:
	if(add_to_rr.s)
		pkg_free(add_to_rr.s);
	return -1;
}


/************************** RRCB functions ******************************/

void rr_checker(struct sip_msg *msg, str *r_param, void *cb_param)
{
	void *direction = 0;
	int upstream = 0;
	int restored;

	if(uac_restore_htable.len > 0
			&& uac_htable_direction(msg, 1, &upstream) != 0) {
		LM_DBG("uac htable direction not found\n");
		return;
	}
	if(uac_auth_cseq_tracking != 0 && uac_cseq_refresh(msg) < 0)
		LM_WARN("failed to refresh uac htable cseq update\n");

	/* check if the request contains stored replacement values */
	restored = restore_uri(msg, &rr_from_param, &restore_from_avp, 1 /*from*/)
			   + restore_uri(msg, &rr_to_param, &restore_to_avp, 0 /*to*/);
	if(uac_restore_htable.len > 0
			&& msg->first_line.u.request.method_value == METHOD_ACK
			&& uac_htable_activate_call(msg) < 0) {
		LM_WARN("failed to activate uac htable entries for ACK\n");
	} else if(uac_restore_htable.len > 0
			  && msg->first_line.u.request.method_value == METHOD_BYE
			  && uac_htable_set_call_expire(msg, uac_restore_htable_rmexpire)
						 < 0) {
		LM_WARN("failed to shorten uac htable expiration for BYE\n");
	}
	if(restored != -2) {
		if(uac_restore_htable.len > 0) {
			direction = upstream ? (void *)(unsigned long)DLG_DIR_UPSTREAM
								 : (void *)(unsigned long)DLG_DIR_DOWNSTREAM;
		}
		/* restore in req performed -> replace in reply */
		/* in callback we need TO/FROM to be parsed- it's already done
		 * by restore_from_to() function */
		if(uac_tmb.register_tmcb(
				   msg, 0, TMCB_RESPONSE_IN, restore_uris_reply, direction, 0)
				!= 1) {
			LM_ERR("failed to install TM callback\n");
			return;
		}
	}
}


/************************** TMCB functions ******************************/

/* replace the entire HDR with the original request */
static inline int restore_uri_reply(struct sip_msg *rpl,
		struct hdr_field *rpl_hdr, struct hdr_field *req_hdr, str *stored_value)

{
	struct lump *l;
	struct to_body *body;
	str new_val;
	int len;
	char *p;

	if(stored_value->len) {
		LM_DBG("stored AVP value is '%.*s'with len '%d'\n", stored_value->len,
				stored_value->s, stored_value->len);
		len = stored_value->len;
		p = stored_value->s;
	} else {
		/* duplicate the new hdr value */
		body = (struct to_body *)req_hdr->parsed;
		/* catch whitespace characters after uri */
		for(p = body->uri.s + body->uri.len, len = 0; isspace(p[len]); len++)
			;
		/* if ">" present after uri (and whitespace), catch it */
		len = p - body->body.s + ((p[len] == '>') ? (len + 1) : 0);
		p = body->body.s;
	}

	new_val.s = pkg_malloc(len);
	if(new_val.s == 0) {
		PKG_MEM_ERROR;
		return -1;
	}
	memcpy(new_val.s, p, len);
	new_val.len = len;

	body = (struct to_body *)rpl_hdr->parsed;

	/* catch whitespace characters after uri */
	for(p = body->uri.s + body->uri.len, len = 0; isspace(p[len]); len++)
		;
	/* if ">" present after uri (and whitespace), catch it */
	len = p - body->body.s + ((p[len] == '>') ? (len + 1) : 0);
	LM_DBG("removing <%.*s>\n", len, body->body.s);
	l = del_lump(rpl, body->body.s - rpl->buf, len, 0);
	if(l == 0) {
		LM_ERR("del lump failed\n");
		pkg_free(new_val.s);
		return -1;
	}

	LM_DBG("inserting <%.*s>\n", new_val.len, new_val.s);
	if(insert_new_lump_after(l, new_val.s, new_val.len, 0) == 0) {
		LM_ERR("insert new lump failed\n");
		pkg_free(new_val.s);
		l->len = 0;
		return -1;
	}

	return 0;
}


/* replace the entire from HDR with the original FROM request */
void restore_uris_reply(struct cell *t, int type, struct tmcb_params *p)
{
	struct sip_msg *req;
	struct sip_msg *rpl;
	int_str avp_value;
	unsigned int direction = 0;

	if(!t || !t->uas.request || !p->rpl)
		return;

	if(*p->param) {
		direction = (unsigned int)(unsigned long)*p->param;
	}

	req = t->uas.request;
	rpl = p->rpl;

	if(((req->msg_flags & FL_USE_UAC_FROM)
			   && (!direction || direction == DLG_DIR_DOWNSTREAM))
			|| ((req->msg_flags & FL_USE_UAC_TO)
					&& direction == DLG_DIR_UPSTREAM)) {
		if(uac_restore_htable.len > 0) {
			if(uac_htable_restore_reply(req, rpl, 1, direction) < 0)
				LM_ERR("failed to restore FROM using htable\n");
		} else {
			/* parse FROM in reply */
			if(parse_from_header(rpl) < 0) {
				LM_ERR("failed to find/parse FROM hdr\n");
				return;
			}

			avp_value.s.len = 0;
			if(restore_from_avp.s) {
				search_first_avp(restore_from_avp_type, restore_from_avp_name,
						&avp_value, 0);
			}

			if(restore_uri_reply(rpl, rpl->from, req->from, &avp_value.s)) {
				LM_ERR("failed to restore FROM\n");
			}
		}
	}

	if(((req->msg_flags & FL_USE_UAC_TO)
			   && (!direction || direction == DLG_DIR_DOWNSTREAM))
			|| ((req->msg_flags & FL_USE_UAC_FROM)
					&& direction == DLG_DIR_UPSTREAM)) {
		if(uac_restore_htable.len > 0) {
			if(uac_htable_restore_reply(req, rpl, 0, direction) < 0)
				LM_ERR("failed to restore TO using htable\n");
		} else {
			/* parse TO in reply */
			if(rpl->to == 0
					&& (parse_headers(rpl, HDR_TO_F, 0) != 0 || rpl->to == 0)) {
				LM_ERR("failed to parse TO hdr\n");
				return;
			}

			avp_value.s.len = 0;
			if(restore_to_avp.s) {
				search_first_avp(restore_to_avp_type, restore_to_avp_name,
						&avp_value, 0);
			}

			if(restore_uri_reply(rpl, rpl->to, req->to, &avp_value.s)) {
				LM_ERR("failed to restore TO\n");
			}
		}
	}
}

/************************** DIALOG CB function ******************************/

static void replace_callback(
		struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct lump *l;
	struct sip_msg *msg;
	struct hdr_field *hdr;
	struct to_body *body;
	str old_uri;
	str new_uri = {0};
	str new_display = {0};
	str buf = STR_NULL;
	char *p;
	unsigned int uac_flag;
	int dlgvar_index = 0;
	int dlgvar_dpindex = 0;
	str *dlgvar_names;
	struct cell *Trans;

	if(!dlg || !_params || _params->direction == DLG_DIR_NONE || !_params->req)
		return;

	uac_flag = (unsigned int)(unsigned long)*(_params->param);
	msg = _params->req;
	if(msg->msg_flags & uac_flag)
		return;

	dlgvar_names = (uac_flag == FL_USE_UAC_FROM) ? from_dlgvar : to_dlgvar;

	/* check the request direction */
	if(((uac_flag == FL_USE_UAC_TO) && _params->direction == DLG_DIR_DOWNSTREAM)
			|| ((uac_flag != FL_USE_UAC_TO)
					&& _params->direction == DLG_DIR_UPSTREAM)) {
		/* replace the TO URI */
		if(msg->to == 0
				&& (parse_headers(msg, HDR_TO_F, 0) != 0 || msg->to == 0)) {
			LM_ERR("failed to parse TO hdr\n");
			return;
		}
		old_uri = ((struct to_body *)msg->to->parsed)->uri;
		hdr = (struct hdr_field *)msg->to;
		body = ((struct to_body *)msg->to->parsed);
	} else {
		/* replace the FROM URI */
		if(parse_from_header(msg) < 0) {
			LM_ERR("failed to find/parse FROM hdr\n");
			return;
		}
		old_uri = ((struct to_body *)msg->from->parsed)->uri;
		hdr = (struct hdr_field *)msg->from;
		body = (struct to_body *)msg->from->parsed;
	}

	if(_params->direction == DLG_DIR_DOWNSTREAM) {
		dlgvar_index = 1;
		dlgvar_dpindex = 3;
		LM_DBG("DOWNSTREAM direction detected - replacing uri"
			   " with the new uri\n");
	} else {
		dlgvar_index = 0;
		dlgvar_dpindex = 2;
		LM_DBG("UPSTREAM direction detected - replacing uri"
			   " with the original uri\n");
	}

	dlg_api.get_dlg_varval(dlg, &dlgvar_names[dlgvar_index], &new_uri);
	if(new_uri.s == NULL) {
		LM_DBG("<%.*s> param not found\n", dlgvar_names[dlgvar_index].len,
				dlgvar_names[dlgvar_index].s);
		return;
	}
	dlg_api.get_dlg_varval(dlg, &dlgvar_names[dlgvar_dpindex], &new_display);
	if(new_display.s == NULL) {
		LM_DBG("<%.*s> param not found\n", dlgvar_names[dlgvar_dpindex].len,
				dlgvar_names[dlgvar_dpindex].s);
		return;
	}

	LM_DBG("Replace [%.*s %.*s] with [%.*s %.*s]\n", body->display.len,
			body->display.s, old_uri.len, old_uri.s, new_display.len,
			new_display.s, new_uri.len, new_uri.s);

	/* deal with display name */
	l = 0;
	/* first remove the existing display */
	if(body->display.s && body->display.len > 0) {
		LM_DBG("removing display [%.*s]\n", body->display.len, body->display.s);
		/* build del lump */
		l = del_lump(msg, body->display.s - msg->buf, body->display.len, 0);
		if(l == 0) {
			LM_ERR("display del lump failed\n");
			return;
		}
	}
	if(new_display.s && new_display.len > 0) {
		LM_DBG("inserting display [%.*s]\n", new_display.len, new_display.s);
		/* add the new display exactly over the deleted one */
		buf.s = pkg_malloc(new_display.len + 2);
		if(buf.s == 0) {
			PKG_MEM_ERROR;
			return;
		}
		memcpy(buf.s, new_display.s, new_display.len);
		buf.len = new_display.len;
		if(l == 0 && (l = get_display_anchor(msg, hdr, body, &buf)) == 0) {
			LM_ERR("failed to insert anchor\n");
			pkg_free(buf.s);
			return;
		}
		if(insert_new_lump_after(l, buf.s, buf.len, 0) == 0) {
			LM_ERR("insert new display lump failed\n");
			pkg_free(buf.s);
			return;
		}
	}

	/* uri update - duplicate the decoded value */
	p = pkg_malloc(new_uri.len);
	if(!p) {
		PKG_MEM_ERROR;
		return;
	}
	memcpy(p, new_uri.s, new_uri.len);

	/* build del/add lumps */
	l = del_lump(msg, old_uri.s - msg->buf, old_uri.len, 0);
	if(l == 0) {
		LM_ERR("del lump failed\n");
		pkg_free(p);
		return;
	}

	if(insert_new_lump_after(l, p, new_uri.len, 0) == 0) {
		LM_ERR("insert new lump failed\n");
		pkg_free(p);
		return;
	}

	/* register tm callback to change replies,
	 * but only if not registered earlier */
	if(!(msg->msg_flags & (FL_USE_UAC_FROM | FL_USE_UAC_TO))
			&& uac_tmb.register_tmcb(msg, 0, TMCB_RESPONSE_IN,
					   restore_uris_reply,
					   (void *)(unsigned long)_params->direction, 0)
					   != 1) {
		LM_ERR("failed to install TM callback\n");
		return;
	}
	msg->msg_flags |= uac_flag;

	if((Trans = uac_tmb.t_gett()) != NULL && Trans != T_UNDEFINED
			&& Trans->uas.request) {
		Trans->uas.request->msg_flags |= uac_flag;
	}

	return;
}


/* helper function to avoid code duplication */
static inline int uac_load_callback_helper(
		struct dlg_cell *dialog, unsigned int uac_flag)
{

	if(dlg_api.register_dlgcb(dialog, DLGCB_REQ_WITHIN,
			   (void *)(unsigned long)replace_callback,
			   (void *)(unsigned long)uac_flag, 0)
			!= 0) {
		LM_ERR("can't register create dialog REQ_WITHIN callback\n");
		return -1;
	}

	if(dlg_api.register_dlgcb(dialog, DLGCB_CONFIRMED,
			   (void *)(unsigned long)replace_callback,
			   (void *)(unsigned long)uac_flag, 0)
			!= 0) {
		LM_ERR("can't register create dialog CONFIRM callback\n");
		return -1;
	}

	if(dlg_api.register_dlgcb(dialog, DLGCB_TERMINATED,
			   (void *)(unsigned long)replace_callback,
			   (void *)(unsigned long)uac_flag, 0)
			!= 0) {
		LM_ERR("can't register create dialog TERMINATED callback\n");
		return -1;
	}
	return 0;
}


/* callback for loading a dialog from database */
static void uac_on_load_callback(
		struct dlg_cell *dialog, int type, struct dlg_cb_params *params)
{

	if(!dialog) {
		LM_ERR("invalid values\n!");
		return;
	}

	/* Note:
	 * We don't have a way to access the real uac flags from the uac_replace_*
	 * method call at this point in time anymore. Therefore we just install a
	 * callback for both FROM and TO replace cases. This might be a bit
	 * inefficient in cases where only one of the functions is used. But as
	 * this applies only e.g. to a proxy restart with running dialogs, it
	 * does not matter. The replace_callback function will just not find
	 * an entry in the dialog variables table and log an error.
	 */
	if(uac_load_callback_helper(dialog, FL_USE_UAC_FROM) != 0) {
		LM_ERR("can't register create callbacks for UAC FROM\n");
		return;
	}
	if(uac_load_callback_helper(dialog, FL_USE_UAC_TO) != 0) {
		LM_ERR("can't register create callbacks for UAC TO\n");
		return;
	}

	LM_DBG("dialog '%p' loaded and callbacks registered\n", dialog);
}


/* initialization of all necessary callbacks to track a dialog */
int uac_init_dlg(void)
{

	memset(&dlg_api, 0, sizeof(struct dlg_binds));

	if(load_dlg_api(&dlg_api) != 0) {
		LM_ERR("can't load dialog API\n");
		return -1;
	}

	if(dlg_api.register_dlgcb(0, DLGCB_LOADED, uac_on_load_callback, 0, 0)
			!= 0) {
		LM_ERR("can't register on load callback\n");
		return -1;
	}
	LM_DBG("loaded dialog API and registered on load callback\n");
	return 0;
}
