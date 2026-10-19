/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
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

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/ut.h"
#include "../../core/pvar.h"
#include "../../core/fmsg.h"
#include "../../core/counters.h"
#include "../../core/route.h"
#include "../../core/cfg/cfg_struct.h"

#include "param_map_parser.h"
#include "log_http.h"
#include "lh_worker.h"
#include "lh_async.h"
#include "lh_format.h"

MODULE_VERSION

group_key_t lh_log_grp = SLIST_HEAD_INITIALIZER(lh_log_grp);
lh_target_list_t lh_targets = SLIST_HEAD_INITIALIZER(lh_targets);
int lh_timeout = 5000;
int lh_chunked = 1;
unsigned int lh_batch_size = 0;
unsigned int lh_batch_delay = 0;
unsigned int lh_queue_size = 0;
str lh_auth_usrpwd = STR_NULL;

stat_var *lh_stat_queue_size = 0;
stat_var *lh_stat_drop_total = 0;
stat_var *lh_stat_err_total = 0;

static str lh_auth_usr = STR_NULL;
static str lh_auth_pwd = STR_NULL;
static char *lh_log_buf = NULL;
static int lh_log_buf_size = 8192;
static struct lh_target *lh_last_target = 0;
static struct lh_target *lh_cur_target = 0;

static int lh_add_target(modparam_t type, void *val)
{
	char *url = (char *)val;
	unsigned int size;
	struct lh_target *target;

	(void)type;

	size = sizeof(struct lh_target) + strlen(url) + 1;
	target = pkg_mallocxz(size);
	if(!target) {
		PKG_MEM_ERROR;
		return -1;
	}
	target->url.s = (char *)target + sizeof(struct lh_target);
	target->url.len = strlen(url);
	memcpy(target->url.s, url, target->url.len + 1);

	if(lh_last_target)
		SLIST_INSERT_AFTER(lh_last_target, target, next);
	else
		SLIST_INSERT_HEAD(&lh_targets, target, next);
	lh_last_target = target;
	return 0;
}

static int lh_parsing_map_cb(
		str *key, str *name, int type, str *stype, void *usrdata)
{
	static struct log_grp_param *last_param = 0, *last_default_param = 0;
	bool default_ = *(bool *)usrdata;
	struct group_key *gkey = 0;
	struct log_grp_param *param;
	log_grp_param_t *param_list = 0;
	struct log_grp_param **last = 0;

	SLIST_FOREACH(gkey, &lh_log_grp, next)
	{
		if(gkey->group_key.len == key->len
				&& strncmp(gkey->group_key.s, key->s, key->len) == 0) {
			break;
		}
	}
	if(!gkey) {
		gkey = pkg_mallocxz(sizeof(struct group_key) + key->len + 1);
		if(!gkey) {
			PKG_MEM_ERROR;
			return -1;
		}
		gkey->group_key.s = (char *)gkey + sizeof(struct group_key);
		gkey->group_key.len = key->len;
		memcpy(gkey->group_key.s, key->s, key->len);
		gkey->group_key.s[key->len] = '\0';
		SLIST_INIT(&gkey->params.default_params.head);
		SLIST_INIT(&gkey->params.params.head);
		SLIST_INSERT_HEAD(&lh_log_grp, gkey, next);
		last_param = 0;
		last_default_param = 0;
	}

	if(!name)
		return 0;

	if(type == LPT_NONE) {
		LM_ERR("unknown type '%.*s' for field '%.*s'\n", stype->len, stype->s,
				name->len, name->s);
		return -1;
	} else if(default_ && type < LH_DEFAULT_START_TYPE) {
		LM_ERR("type '%.*s' is not allowed in default_map (field '%.*s')\n",
				stype->len, stype->s, name->len, name->s);
		return -1;
	} else if(!default_ && type >= LH_DEFAULT_START_TYPE) {
		LM_ERR("type '%.*s' is not allowed in map (field '%.*s')\n", stype->len,
				stype->s, name->len, name->s);
		return -1;
	} else if(type == LPDT_PLACEHOLDER) {
		if(!pv_cache_get(stype)) {
			LM_ERR("invalid pseudo-variable '%.*s' for field '%.*s'\n",
					stype->len, stype->s, name->len, name->s);
			return -1;
		}
	}

	param = pkg_mallocxz(
			sizeof(struct log_grp_param) + name->len + stype->len + 2);
	if(!param) {
		PKG_MEM_ERROR;
		return -1;
	}
	param->name.s = (char *)param + sizeof(struct log_grp_param);
	param->name.len = name->len;
	memcpy(param->name.s, name->s, name->len);
	param->name.s[name->len] = '\0';
	param->type = type;
	param->stype.s = param->name.s + param->name.len + 1;
	param->stype.len = stype->len;
	memcpy(param->stype.s, stype->s, stype->len);
	param->stype.s[stype->len] = '\0';

	if(default_) {
		param_list = &gkey->params.default_params;
		last = &last_default_param;
	} else {
		param_list = &gkey->params.params;
		last = &last_param;
	}
	if(*last)
		SLIST_INSERT_AFTER(*last, param, next);
	else
		SLIST_INSERT_HEAD(&param_list->head, param, next);
	*last = param;
	param_list->size++;
	return 0;
}

static int lh_fill_auth_usrpwd(void)
{
	if(lh_auth_usrpwd.s)
		pkg_free(lh_auth_usrpwd.s);
	lh_auth_usrpwd.len = lh_auth_usr.len + lh_auth_pwd.len + 1;
	lh_auth_usrpwd.s = pkg_malloc(lh_auth_usrpwd.len + 1);
	if(!lh_auth_usrpwd.s) {
		PKG_MEM_ERROR;
		lh_auth_usrpwd.len = 0;
		return -1;
	}
	memcpy(lh_auth_usrpwd.s, lh_auth_usr.s, lh_auth_usr.len);
	lh_auth_usrpwd.s[lh_auth_usr.len] = ':';
	memcpy(lh_auth_usrpwd.s + lh_auth_usr.len + 1, lh_auth_pwd.s,
			lh_auth_pwd.len);
	lh_auth_usrpwd.s[lh_auth_usrpwd.len] = '\0';
	return 0;
}

static int lh_set_auth_str(str *dst, char *val)
{
	if(dst->s)
		pkg_free(dst->s);
	dst->len = strlen(val);
	dst->s = pkg_malloc(dst->len + 1);
	if(!dst->s) {
		PKG_MEM_ERROR;
		dst->len = 0;
		return -1;
	}
	memcpy(dst->s, val, dst->len + 1);
	return 0;
}

static int lh_auth_username(modparam_t type, void *val)
{
	(void)type;
	if(lh_set_auth_str(&lh_auth_usr, (char *)val) < 0)
		return -1;
	if(lh_auth_pwd.s)
		return lh_fill_auth_usrpwd();
	return 0;
}

static int lh_auth_password(modparam_t type, void *val)
{
	(void)type;
	if(lh_set_auth_str(&lh_auth_pwd, (char *)val) < 0)
		return -1;
	if(lh_auth_usr.s)
		return lh_fill_auth_usrpwd();
	return 0;
}

static int lh_default_map_parse(modparam_t type, void *val)
{
	bool is_default_map = true;
	(void)type;
	return parse_log_mapping((char *)val, lh_parsing_map_cb, &is_default_map);
}

static int lh_map_parse(modparam_t type, void *val)
{
	bool is_default_map = false;
	(void)type;
	return parse_log_mapping((char *)val, lh_parsing_map_cb, &is_default_map);
}

static int lh_format_param(modparam_t type, void *val)
{
	(void)type;
	return lh_format_parse((char *)val);
}

/* clang-format off */
static param_export_t params[] = {
	{"target", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_add_target},
	{"format", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_format_param},
	{"timeout", PARAM_INT, &lh_timeout},
	{"chunked", PARAM_INT, &lh_chunked},
	{"batch_size", PARAM_INT, &lh_batch_size},
	{"batch_delay", PARAM_INT, &lh_batch_delay},
	{"queue_size", PARAM_INT, &lh_queue_size},
	{"auth_username", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_auth_username},
	{"auth_password", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_auth_password},
	{"default_map", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_default_map_parse},
	{"map", PARAM_STRING | PARAM_USE_FUNC, (void *)lh_map_parse},
	{0, 0, 0}
};
/* clang-format on */

static inline uint64_t lh_now_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* append a string to the per-call buffer; returns pointer to the copy */
static char *lh_buf_append(char **cur, const char *s, int len)
{
	char *p = *cur;
	if(len < 0 || (lh_log_buf + lh_log_buf_size) - p < len + 1) {
		LM_ERR("log buffer too small (%d bytes), value truncated\n",
				lh_log_buf_size);
		return NULL;
	}
	memcpy(p, s, len);
	p[len] = '\0';
	*cur += len + 1;
	return p;
}

static int lh_fill_default_param(struct sip_msg *_m,
		struct log_grp_param *param, async_param_value_t *value, char **cur)
{
	pv_spec_t *pvspec;
	pv_value_t pvvalue;
	const char *rname;

	switch(param->type) {
		case LPDT_ROUTENAME:
			rname = get_cfg_crt_route_name();
			value->type = LPT_STRING;
			value->u.string.s = (char *)(rname ? rname : "");
			value->u.string.len = strlen(value->u.string.s);
			break;
		case LPDT_TIMESTAMP:
			value->type = LPT_DATE;
			value->u.date = lh_now_ms();
			break;
		case LPDT_PLACEHOLDER:
			pvspec = pv_cache_get(&param->stype);
			memset(&pvvalue, 0, sizeof(pvvalue));
			value->type = LPT_STRING;
			value->u.string.s = "";
			value->u.string.len = 0;
			if(!pvspec || pv_get_spec_value(_m, pvspec, &pvvalue) < 0)
				break;
			if((pvvalue.flags & PV_VAL_NULL) || !(pvvalue.flags & PV_VAL_STR))
				break;
			value->u.string.s =
					lh_buf_append(cur, pvvalue.rs.s, pvvalue.rs.len);
			if(!value->u.string.s)
				return -1;
			value->u.string.len = pvvalue.rs.len;
			break;
		case LPDT_IMMEDIATE:
			value->type = LPT_STRING;
			value->u.string = param->stype;
			break;
		default:
			return -1;
	}
	return 0;
}

static int lh_fill_param(struct sip_msg *_m, struct log_grp_param *param,
		char *arg, async_param_value_t *value, char **cur)
{
	str sarg;
	int ival;
	char *end;
	struct tm tm_;
	pv_elem_t *pve;
	int len;

	sarg.s = arg;
	sarg.len = strlen(arg);

	switch(param->type) {
		case LPT_BOOL:
			value->type = LPT_BOOL;
			if(str2sint(&sarg, &ival) == 0) {
				value->u.cond = (ival != 0);
			} else if(strcasecmp("true", arg) == 0) {
				value->u.cond = true;
			} else if(strcasecmp("false", arg) == 0) {
				value->u.cond = false;
			} else {
				return -1;
			}
			break;
		case LPT_NUMBER:
			if(str2sint(&sarg, &ival) < 0)
				return -1;
			value->type = LPT_NUMBER;
			value->u.num = ival;
			break;
		case LPT_STRING:
			if(pv_parse_format(&sarg, &pve) < 0) {
				LM_ERR("wrong format [%s]\n", arg);
				return -1;
			}
			len = lh_log_buf_size - (*cur - lh_log_buf);
			if(len <= 0 || pv_printf(_m, pve, *cur, &len) < 0) {
				pv_elem_free_all(pve);
				LM_ERR("failed to evaluate [%s] (buffer left %d)\n", arg, len);
				return -1;
			}
			pv_elem_free_all(pve);
			value->type = LPT_STRING;
			value->u.string.s = *cur;
			value->u.string.len = len;
			*cur += len;
			if(*cur < lh_log_buf + lh_log_buf_size)
				*(*cur)++ = '\0';
			break;
		case LPT_IPADDR:
			value->type = LPT_IPADDR;
			if(strchr(arg, ':')) {
				value->u.addr.af = AF_INET6;
				value->u.addr.len = 16;
			} else {
				value->u.addr.af = AF_INET;
				value->u.addr.len = 4;
			}
			if(inet_pton(value->u.addr.af, arg, value->u.addr.u.addr) != 1)
				return -1;
			break;
		case LPT_DATE:
			memset(&tm_, 0, sizeof(struct tm));
			end = strptime(arg, "%Y.%m.%d %H:%M:%S", &tm_);
			if(!end || *end)
				return -1;
			tm_.tm_isdst = -1;
			value->type = LPT_DATE;
			value->u.date = (uint64_t)mktime(&tm_) * 1000;
			break;
		case LPT_FLOAT:
			value->type = LPT_FLOAT;
			value->u.flp = strtof(arg, &end);
			if(end == arg || *end)
				return -1;
			break;
		default:
			return -1;
	}
	return 0;
}

static int lh_group_log(
		struct sip_msg *_m, struct group_key *key, int argc, action_u_t argv[])
{
	int ret;
	int size;
	int i = 0, j = 0;
	char *cur = lh_log_buf;
	async_param_value_t *params;
	struct log_grp_param *param;

	if(SLIST_EMPTY(&lh_targets)) {
		LM_ERR("no targets configured, specify at least one 'target' "
			   "modparam\n");
		return -1;
	}

	if(key->params.params.size != argc) {
		LM_ERR("group '%s': number of arguments (%d) does not match number "
			   "of fields in map (%d)\n",
				key->group_key.s, argc, key->params.params.size);
		return -1;
	}

	size = key->params.params.size + key->params.default_params.size;
	params = pkg_malloc(sizeof(async_param_value_t) * size);
	if(!params) {
		PKG_MEM_ERROR;
		return -1;
	}

	SLIST_FOREACH(param, &key->params.default_params.head, next)
	{
		if(lh_fill_default_param(_m, param, &params[i], &cur) < 0)
			goto err;
		i++;
	}
	SLIST_FOREACH(param, &key->params.params.head, next)
	{
		if(lh_fill_param(_m, param, argv[j].u.string, &params[i], &cur) < 0)
			goto err;
		i++;
		j++;
	}

	ret = lh_async_send(key, params, size);
	pkg_free(params);
	if(ret < 0) {
		if(_m && _m->callid && _m->callid->body.s) {
			LM_ERR("failed to queue log entry (%d), callid: %.*s\n", ret,
					_m->callid->body.len, _m->callid->body.s);
		} else {
			LM_ERR("failed to queue log entry (%d)\n", ret);
		}
		return -1;
	}
	return 1;
err:
	LM_ERR("group '%s': field '%s' (type %s) has an invalid value, cfg "
		   "line %d\n",
			key->group_key.s, param ? param->name.s : "",
			param ? param->stype.s : "", get_cfg_crt_line());
	pkg_free(params);
	return -1;
}

static int w_http_log(struct sip_msg *_m, int argc, action_u_t argv[])
{
	str gkey;
	struct group_key *key;

	if(argc < 1) {
		LM_ERR("logging group parameter is missing\n");
		return -1;
	}

	gkey.s = argv[0].u.string;
	gkey.len = strlen(gkey.s);
	SLIST_FOREACH(key, &lh_log_grp, next)
	{
		if(gkey.len == key->group_key.len
				&& strncmp(key->group_key.s, gkey.s, gkey.len) == 0) {
			return lh_group_log(_m, key, argc - 1, argv + 1);
		}
	}

	LM_ERR("log group '%.*s' not found\n", gkey.len, gkey.s);
	return -1;
}

/* clang-format off */
static cmd_export_t cmds[] = {
	{"http_log", (cmd_function)w_http_log, VAR_PARAM_NO, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static stat_export_t mod_stats[] = {
	{"lh_queue_size", 0, &lh_stat_queue_size},
	{"lh_drop_total", 0, &lh_stat_drop_total},
	{"lh_err_total", 0, &lh_stat_err_total},
	{0, 0, 0}
};
/* clang-format on */

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

/* clang-format off */
struct module_exports exports = {
	"log_http", /* module name */
	DEFAULT_DLFLAGS,    /* dlopen flags */
	cmds,               /* exported functions */
	params,             /* exported parameters */
	0,                  /* exported rpc functions */
	0,                  /* exported pseudo-variables */
	0,                  /* response handling function */
	mod_init,           /* module init function */
	child_init,         /* per-child init function */
	mod_destroy         /* module destroy function */
};
/* clang-format on */

static int mod_init(void)
{
	if(SLIST_EMPTY(&lh_targets)) {
		LM_ERR("no targets configured, specify at least one 'target' "
			   "modparam\n");
		return -1;
	}
	if(SLIST_EMPTY(&lh_log_grp)) {
		LM_ERR("no log groups configured, specify at least one 'map' "
			   "modparam\n");
		return -1;
	}
	if(lh_timeout <= 0) {
		LM_ERR("timeout must be a positive number of milliseconds\n");
		return -1;
	}

	lh_format_set_default();
	if(lh_format->init && lh_format->init() < 0)
		return -1;

	lh_log_buf = (char *)pkg_malloc(lh_log_buf_size + 1);
	if(lh_log_buf == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}

	if(faked_msg_init() < 0) {
		LM_ERR("failed to init faked sip msg\n");
		return -1;
	}

	register_procs(1);
	cfg_register_child(1);

#ifdef STATISTICS
	if(register_module_stats(exports.name, mod_stats) != 0) {
		LM_ERR("failed to register core statistics\n");
		return -1;
	}
#endif

	return 0;
}

static int child_init(int rank)
{
	int pid;

	if(rank == PROC_INIT) {
		if(lh_worker_init_pair_sockets() < 0)
			return -1;
		return 0;
	}

	if(rank > 0) {
		/* SIP worker: only sends */
		lh_worker_close_sockets_parent();
		return 0;
	}

	if(rank != PROC_MAIN)
		return 0;

	pid = fork_process(PROC_NOCHLDINIT, "log_http sender", 1);
	if(pid < 0)
		return -1;

	if(pid == 0) {
		/* sender process */
		if(cfg_child_init())
			return -1;

		lh_worker_close_sockets_child();
		lh_worker_run();
		/* reached only on init failure */
		exit(-1);
	}

	return 0;
}

static void mod_destroy(void)
{
	struct group_key *e, *bak;
	struct lh_target *t, *tbak;

	lh_worker_destroy();
	lh_free_queue();
	lh_format_destroy();

	SLIST_FOREACH_SAFE(e, &lh_log_grp, next, bak)
	{
		log_grp_entry_t *group = &e->params;
		struct log_grp_param *param, *tparam;

		SLIST_FOREACH_SAFE(param, &group->default_params.head, next, tparam)
		{
			pkg_free(param);
		}
		SLIST_FOREACH_SAFE(param, &group->params.head, next, tparam)
		{
			pkg_free(param);
		}
		pkg_free(e);
	}

	SLIST_FOREACH_SAFE(t, &lh_targets, next, tbak)
	{
		pkg_free(t);
	}

	if(lh_log_buf)
		pkg_free(lh_log_buf);
	if(lh_auth_pwd.s)
		pkg_free(lh_auth_pwd.s);
	if(lh_auth_usr.s)
		pkg_free(lh_auth_usr.s);
	if(lh_auth_usrpwd.s)
		pkg_free(lh_auth_usrpwd.s);
}

/* round-robin over the configured targets */
struct lh_target *lh_get_next_target(void)
{
	if(!lh_cur_target || !SLIST_NEXT(lh_cur_target, next)) {
		lh_cur_target = SLIST_FIRST(&lh_targets);
	} else {
		lh_cur_target = SLIST_NEXT(lh_cur_target, next);
	}
	return lh_cur_target;
}

static inline int lh_type_is(str *type, const char *name)
{
	int len = strlen(name);
	return type->len == len && strncmp(type->s, name, len) == 0;
}

int lh_get_param_type(str *type)
{
	if(lh_type_is(type, "text"))
		return LPT_STRING;
	if(lh_type_is(type, "inet"))
		return LPT_IPADDR;
	if(lh_type_is(type, "num"))
		return LPT_NUMBER;
	if(lh_type_is(type, "bool"))
		return LPT_BOOL;
	if(lh_type_is(type, "date"))
		return LPT_DATE;
	if(lh_type_is(type, "float"))
		return LPT_FLOAT;

	if(lh_type_is(type, "timestamp"))
		return LPDT_TIMESTAMP;
	if(lh_type_is(type, "route_name"))
		return LPDT_ROUTENAME;
	if(type->len > 0 && *type->s == '$') {
		return LPDT_PLACEHOLDER;
	}
	if(type->len > 0 && *type->s == '&') {
		type->s++;
		type->len--;
		return LPDT_IMMEDIATE;
	}
	return LPT_NONE;
}
