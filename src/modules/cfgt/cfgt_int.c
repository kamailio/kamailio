/**
 *
 * Copyright (C) 2015 Victor Seva (sipwise.com)
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
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include "../../core/events.h"
#include "../../core/strutils.h"
#include "../../core/pvar.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/parser/msg_parser.h"

#include "cfgt_int.h"
#include "cfgt_json.h"

static str _cfgt_route_prefix[] = {str_init("start|"), str_init("exit|"),
		str_init("drop|"), str_init("return|"), {0, 0}};
cfgt_node_p _cfgt_node = NULL;
cfgt_hash_p _cfgt_uuid = NULL;
str cfgt_hdr_prefix = {"NGCP%", 5};
str cfgt_basedir = {"/tmp", 4};
int cfgt_mask = CFGT_DP_ALL;
int not_sip = 0;

int _cfgt_get_filename(int msgid, str uuid, str *dest, int *dir);

static int shm_str_hash_alloc(struct str_hash_table *ht, int size)
{
	ht->table = shm_malloc(sizeof(struct str_hash_head) * size);
	if(!ht->table) {
		SHM_MEM_ERROR;
		return -1;
	}

	ht->size = size;
	return 0;
}

static int _cfgt_init_hashtable(struct str_hash_table *ht)
{
	if(shm_str_hash_alloc(ht, CFGT_HASH_SIZE) != 0)
		return -1;

	str_hash_init(ht);

	return 0;
}

void _cfgt_remove_report(const str *scen)
{
	str dest = STR_NULL;
	str filepath = STR_NULL;
	int dir = 0;
	int len;
	struct stat sb;
	DIR *folder = NULL;
	struct dirent *next_file = NULL;

	if(_cfgt_get_filename(0, *scen, &dest, &dir) < 0) {
		LM_ERR("can't build filename for uuid: %.*s\n", scen->len, scen->s);
		return;
	}
	dest.s[dir] = '\0';

	if(stat(dest.s, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		filepath.s = (char *)pkg_malloc(sizeof(char) * dest.len + 1);
		if(filepath.s == NULL) {
			PKG_MEM_ERROR;
			goto end;
		}
		if((folder = opendir(dest.s)) == NULL) {
			LM_ERR("can't open dir: %s", strerror(errno));
			goto end;
		}
		while((next_file = readdir(folder)) != NULL) {
			len = strlen(next_file->d_name);
			if(len <= 2) {
				if(strncmp(next_file->d_name, "..", 2) == 0
						|| strncmp(next_file->d_name, ".", 1) == 0) {
					continue;
				}
			}
			snprintf(filepath.s, dest.len + 1, "%s/%s", dest.s,
					next_file->d_name);
			if(remove(filepath.s) < 0) {
				LM_ERR("failed removing file: %s\n", strerror(errno));
			} else {
				LM_DBG("removed report file: %s\n", filepath.s);
			}
		}
		if(closedir(folder) < 0) {
			LM_ERR("can't close dir: %s", strerror(errno));
		}
		if(remove(dest.s) < 0) {
			LM_ERR("failed removing dir: %s\n", strerror(errno));
		} else {
			LM_DBG("removed report dir: %s\n", dest.s);
		}
	} else {
		LM_DBG("dir %s not found\n", dest.s);
	}

end:
	if(filepath.s)
		pkg_free(filepath.s);
	if(dest.s)
		pkg_free(dest.s);
}

int _cfgt_remove_uuid(const str *uuid, int remove_report)
{
	struct str_hash_head *head;
	struct str_hash_entry *entry, *back;
	int i;
	int res = -1;

	if(_cfgt_uuid == NULL)
		return res;
	if(uuid) {
		lock_get(&_cfgt_uuid->lock);
		entry = str_hash_get(&_cfgt_uuid->hash, uuid->s, uuid->len);
		if(entry) {
			str_hash_del(entry);
			if(remove_report)
				_cfgt_remove_report(&entry->key);
			shm_free(entry->key.s);
			shm_free(entry);
			LM_DBG("uuid[%.*s] removed from hash\n", uuid->len, uuid->s);
			res = 0;
		} else {
			LM_DBG("uuid[%.*s] not found in hash\n", uuid->len, uuid->s);
		}
		lock_release(&_cfgt_uuid->lock);
	} else {
		lock_get(&_cfgt_uuid->lock);
		for(i = 0; i < CFGT_HASH_SIZE; i++) {
			head = _cfgt_uuid->hash.table + i;
			clist_foreach_safe(head, entry, back, next)
			{
				str_hash_del(entry);
				if(remove_report)
					_cfgt_remove_report(&entry->key);
				LM_DBG("uuid[%.*s] removed from hash\n", entry->key.len,
						entry->key.s);
				shm_free(entry->key.s);
				shm_free(entry);
			}
		}
		lock_release(&_cfgt_uuid->lock);
		res = 0;
		LM_DBG("remove all uuids. done\n");
	}
	return res;
}

int _cfgt_get_uuid_id(cfgt_node_p node)
{
	struct str_hash_entry *entry;

	if(_cfgt_uuid == NULL || node == NULL || node->uuid.len == 0)
		return -1;
	lock_get(&_cfgt_uuid->lock);
	entry = str_hash_get(&_cfgt_uuid->hash, node->uuid.s, node->uuid.len);
	if(entry) {
		entry->u.n = entry->u.n + 1;
		node->msgid = entry->u.n;
	} else {
		entry = shm_malloc(sizeof(struct str_hash_entry));
		if(entry == NULL) {
			lock_release(&_cfgt_uuid->lock);
			SHM_MEM_ERROR;
			return -1;
		}
		if(shm_str_dup(&entry->key, &node->uuid) != 0) {
			lock_release(&_cfgt_uuid->lock);
			shm_free(entry);
			return -1;
		}
		entry->u.n = 1;
		node->msgid = 1;
		LM_DBG("Add new entry[%.*s]\n", node->uuid.len, node->uuid.s);
		str_hash_add(&_cfgt_uuid->hash, entry);
	}
	lock_release(&_cfgt_uuid->lock);
	LM_DBG("msgid:[%d]\n", node->msgid);
	return 1;
}

int _cfgt_get_hdr_helper(struct sip_msg *msg, str *res, int mode)
{
	struct hdr_field *hf;
	char *delimiter, *end;
	str tmp = STR_NULL;

	if(msg == NULL || (mode == 0 && res == NULL))
		return -1;

	if(parse_headers(msg, HDR_CALLID_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	hf = msg->callid;
	if(!hf)
		return 1;

	if(strncmp(hf->body.s, cfgt_hdr_prefix.s, cfgt_hdr_prefix.len) == 0) {
		tmp.s = hf->body.s + cfgt_hdr_prefix.len;
		delimiter = tmp.s - 1;
		LM_DBG("Prefix detected. delimiter[%c]\n", *delimiter);
		if(mode == 0) {
			end = strchr(tmp.s, *delimiter);
			if(end) {
				tmp.len = end - tmp.s;
				if(pkg_str_dup(res, &tmp) < 0) {
					LM_ERR("error copying header\n");
					return -1;
				}
				LM_DBG("cfgtest uuid:[%.*s]\n", res->len, res->s);
				return 0;
			}
		} else {
			tmp.len = res->len;
			LM_DBG("tmp[%.*s] res[%.*s]\n", tmp.len, tmp.s, res->len, res->s);
			return STR_EQ(tmp, *res);
		}
	}
	return 2; /* not found */
}

int _cfgt_get_hdr(struct sip_msg *msg, str *res)
{
	return _cfgt_get_hdr_helper(msg, res, 0);
}

int _cfgt_cmp_hdr(struct sip_msg *msg, str *res)
{
	return _cfgt_get_hdr_helper(msg, res, 1);
}

cfgt_node_p cfgt_create_node(struct sip_msg *msg)
{
	cfgt_node_p node;

	node = (cfgt_node_p)pkg_malloc(sizeof(cfgt_node_t));
	if(node == NULL) {
		PKG_MEM_ERROR;
		return node;
	}
	memset(node, 0, sizeof(cfgt_node_t));
	srjson_InitDoc(&node->jdoc, NULL);
	if(msg) {
		node->msgid = msg->id;
		LM_DBG("msgid:%d\n", node->msgid);
		if(_cfgt_get_hdr(msg, &node->uuid) != 0 || node->uuid.len == 0) {
			LM_ERR("cannot get value of cfgtest uuid header!!\n");
			goto error;
		}
	}
	node->jdoc.root = srjson_CreateObject(&node->jdoc);
	if(node->jdoc.root == NULL) {
		LM_ERR("cannot create json root\n");
		goto error;
	}
	node->flow = srjson_CreateArray(&node->jdoc);
	if(node->flow == NULL) {
		LM_ERR("cannot create json object\n");
		goto error;
	}
	srjson_AddItemToObject(&node->jdoc, node->jdoc.root, "flow\0", node->flow);
	node->in = srjson_CreateArray(&node->jdoc);
	if(node->in == NULL) {
		LM_ERR("cannot create json object\n");
		goto error;
	}
	srjson_AddItemToObject(&node->jdoc, node->jdoc.root, "sip_in\0", node->in);
	node->out = srjson_CreateArray(&node->jdoc);
	if(node->out == NULL) {
		LM_ERR("cannot create json object\n");
		goto error;
	}
	srjson_AddItemToObject(
			&node->jdoc, node->jdoc.root, "sip_out\0", node->out);
	LM_DBG("node created\n");
	return node;

error:
	srjson_DestroyDoc(&node->jdoc);
	pkg_free(node);
	return NULL;
}

void _cfgt_remove_node(cfgt_node_p node)
{
	if(!node)
		return;
	srjson_DestroyDoc(&node->jdoc);
	if(node->uuid.s)
		pkg_free(node->uuid.s);
	while(node->flow_head) {
		node->route = node->flow_head;
		node->flow_head = node->route->next;
		pkg_free(node->route);
		node->route = NULL;
	}
	pkg_free(node);
}

int _cfgt_get_filename(int msgid, str uuid, str *dest, int *dir)
{
	int lid;
	char buff_id[INT2STR_MAX_LEN];
	char *sid;
	char *format = "%.*s%.*s/%.*s.json";
	if(dest == NULL || uuid.len == 0)
		return -1;

	dest->len = cfgt_basedir.len + uuid.len;
	if(cfgt_basedir.s[cfgt_basedir.len - 1] != '/') {
		dest->len = dest->len + 1;
		format = "%.*s/%.*s/%.*s.json";
	}
	(*dir) = dest->len;
	sid = sint2strbuf(msgid, buff_id, INT2STR_MAX_LEN, &lid);
	dest->len += lid + 6;
	dest->s = (char *)pkg_malloc((dest->len * sizeof(char) + 1));
	if(dest->s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	snprintf(dest->s, dest->len + 1, format, cfgt_basedir.len, cfgt_basedir.s,
			uuid.len, uuid.s, lid, sid);
	return 0;
}

int _cfgt_node2json(cfgt_node_p node)
{
	srjson_t *jobj;

	if(!node)
		return -1;
	jobj = srjson_CreateStr(&node->jdoc, node->uuid.s, node->uuid.len);
	if(jobj == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}
	srjson_AddItemToObject(&node->jdoc, node->jdoc.root, "uuid\0", jobj);

	jobj = srjson_CreateNumber(&node->jdoc, (double)node->msgid);
	if(jobj == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}
	srjson_AddItemToObject(&node->jdoc, node->jdoc.root, "msgid\0", jobj);
	return 0;
}

void cfgt_save_node(cfgt_node_p node)
{
	FILE *fp;
	str dest = STR_NULL;
	int dir = 0;
	struct stat sb;
	if(_cfgt_get_filename(node->msgid, node->uuid, &dest, &dir) < 0) {
		LM_ERR("can't build filename\n");
		return;
	}
	dest.s[dir] = '\0';
	LM_DBG("dir [%s]\n", dest.s);
	if(stat(dest.s, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		LM_DBG("dir [%s] already exists\n", dest.s);
	} else if(mkdir(dest.s, S_IRWXO | S_IXGRP | S_IRWXU) < 0) {
		LM_ERR("failed to make directory: %s\n", strerror(errno));
		pkg_free(dest.s);
		return;
	}
	dest.s[dir] = '/';
	fp = fopen(dest.s, "w");
	LM_DBG("file [%s]\n", dest.s);
	if(fp) {
		pkg_free(dest.s);
		dest.s = srjson_Print(&node->jdoc, node->jdoc.root);
		if(dest.s == NULL) {
			LM_ERR("Cannot get the json string\n");
			fclose(fp);
			return;
		}
		if(fputs(dest.s, fp) < 0) {
			LM_ERR("failed writing to file: %s\n", strerror(errno));
		}
		fclose(fp);
		node->jdoc.free_fn(dest.s);
		LM_INFO("*** node uuid:[%.*s] id:[%d] saved ***\n",
				STR_FMT(&node->uuid), node->msgid);
	} else {
		LM_ERR("Can't open file [%s] to write\n", dest.s);
		pkg_free(dest.s);
	}
}

void _cfgt_print_node(cfgt_node_p node, int json)
{
	char *buf = NULL;
	cfgt_str_list_p route;

	if(!node)
		return;
	if(node->flow_head) {
		route = node->flow_head;
		while(route) {
			if(route == node->route)
				LM_DBG("[--[%.*s][%d]--]\n", route->s.len, route->s.s,
						route->type);
			else
				LM_DBG("[%.*s][%d]\n", route->s.len, route->s.s, route->type);
			route = route->next;
		}
	} else
		LM_DBG("flow:empty\n");
	if(json) {
		buf = srjson_PrintUnformatted(&node->jdoc, node->jdoc.root);
		if(buf == NULL) {
			LM_ERR("Cannot get the json string\n");
			return;
		}
		LM_DBG("node[%p]: id:[%d] uuid:[%.*s] info:[%s]\n", node, node->msgid,
				node->uuid.len, node->uuid.s, buf);
		node->jdoc.free_fn(buf);
	}
}

int _cfgt_set_dump(struct sip_msg *msg, cfgt_node_p node, str *flow)
{
	srjson_t *f, *vars;

	if(node == NULL || flow == NULL)
		return -1;

	vars = srjson_CreateObject(&node->jdoc);
	if(vars == NULL) {
		LM_ERR("cannot create json object\n");
		return -1;
	}

	/* TODO: during cfgt_get_json some errors are generated due to missing
	values in some routes. Skip the function when node->route->s.s equals
	'dialog:failed' partially solve the issue */
	if(cfgt_get_json(msg, 30, &node->jdoc, vars) < 0) {
		LM_ERR("cannot get var info\n");
		return -1;
	}

	f = srjson_CreateObject(&node->jdoc);
	if(f == NULL) {
		LM_ERR("cannot create json object\n");
		srjson_Delete(&node->jdoc, vars);
		return -1;
	}
	srjson_AddStrItemToObject(&node->jdoc, f, flow->s, flow->len, vars);
	srjson_AddItemToArray(&node->jdoc, node->flow, f);
	LM_DBG("node[%.*s] flow created\n", flow->len, flow->s);
	return 0;
}

void _cfgt_set_type(cfgt_str_list_p route, struct action *a)
{
	switch(a->type) {
		case DROP_T:
			if(a->val[1].u.number & DROP_R_F) {
				route->type = CFGT_DROP_D;
				LM_DBG("set[%.*s]->CFGT_DROP_D\n", route->s.len, route->s.s);
			}
			if(a->val[1].u.number & RETURN_R_F) {
				route->type = CFGT_DROP_R;
				LM_DBG("set[%.*s]->CFGT_DROP_R\n", route->s.len, route->s.s);
			} else {
				route->type = CFGT_DROP_E;
				LM_DBG("set[%.*s]->CFGT_DROP_E\n", route->s.len, route->s.s);
			}
			break;
		case ROUTE_T:
			route->type = CFGT_ROUTE;
			LM_DBG("set[%.*s]->CFGT_ROUTE\n", route->s.len, route->s.s);
			break;
		default:
			if(route->type != CFGT_DROP_E) {
				route->type = CFGT_DROP_R;
				LM_DBG("[%.*s] no relevant action: CFGT_DROP_R[%d]\n",
						route->s.len, route->s.s, a->type);
			} else {
				LM_DBG("[%.*s] already set to CFGT_DROP_E[%d]\n", route->s.len,
						route->s.s, a->type);
			}
			break;
	}
}

int _cfgt_add_routename(cfgt_node_p node, struct action *a, str *routename)
{
	cfgt_str_list_p route;
	int ret = 0;

	if(!node->route) /* initial */
	{
		node->route = pkg_malloc(sizeof(cfgt_str_list_t));
		if(!node->route) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(node->route, 0, sizeof(cfgt_str_list_t));
		node->flow_head = node->route;
		node->route->type = CFGT_ROUTE;
		ret = 1;
	} else {
		LM_DBG("actual routename:[%.*s][%d]\n", node->route->s.len,
				node->route->s.s, node->route->type);
		if(node->route->prev) {
			if(node->route->prev->prev)
				LM_DBG("prev prev routename:[%.*s][%d]\n",
						node->route->prev->prev->s.len,
						node->route->prev->prev->s.s,
						node->route->prev->prev->type);
			LM_DBG("prev routename:[%.*s][%d]\n", node->route->prev->s.len,
					node->route->prev->s.s, node->route->prev->type);
		}
		if(node->route->next)
			LM_DBG("next routename:[%.*s][%d]\n", node->route->next->s.len,
					node->route->next->s.s, node->route->next->type);
		if(STR_EQ(*routename, node->route->s)) {
			LM_DBG("same route\n");
			_cfgt_set_type(node->route, a);
			return 2;
		} else if(node->route->prev) {
			if(STR_EQ(*routename, node->route->prev->s)) {
				LM_DBG("back to prev route[%.*s]\n", node->route->prev->s.len,
						node->route->prev->s.s);
				_cfgt_set_type(node->route->prev, a);
				return 3;
			} else if(node->route->prev->prev
					  && STR_EQ(*routename, node->route->prev->prev->s)) {
				LM_DBG("back to prev prev route[%.*s]\n",
						node->route->prev->prev->s.len,
						node->route->prev->prev->s.s);
				_cfgt_set_type(node->route->prev->prev, a);
				return 3;
			}
		}
		route = pkg_malloc(sizeof(cfgt_str_list_t));
		if(!route) {
			PKG_MEM_ERROR;
			return -1;
		}
		memset(route, 0, sizeof(cfgt_str_list_t));
		if(route->prev && node->route->prev) {
			route->prev->prev = node->route->prev;
		}
		route->prev = node->route;
		node->route->next = route;
		node->route = route;
		_cfgt_set_type(node->route, a);
	}
	node->route->s.s = routename->s;
	node->route->s.len = routename->len;
	LM_DBG("add[%d] route:[%.*s]\n", ret, node->route->s.len, node->route->s.s);
	_cfgt_print_node(node, 0);
	return ret;
}

void _cfgt_del_routename(cfgt_node_p node)
{
	if(node->route->next != NULL) {
		LM_ERR("wtf!! route->next[%p] not null!!\n", node->route->next);
		_cfgt_print_node(node, 0);
	}
	LM_DBG("del route[%.*s]\n", node->route->s.len, node->route->s.s);
	node->route = node->route->prev;
	pkg_free(node->route->next);
	node->route->next = NULL;
}
/* dest has to be freed */
int _cfgt_node_get_flowname(cfgt_str_list_p route, int *indx, str *dest)
{
	int i;
	if(route == NULL)
		return -1;
	LM_DBG("routename:[%.*s][%d]\n", route->s.len, route->s.s, route->type);
	if(indx)
		i = *indx;
	else
		i = route->type - 1;
	if(str_append(&_cfgt_route_prefix[i], &route->s, dest) < 0) {
		LM_ERR("Cannot create route name\n");
		return -1;
	}
	return 0;
}

int _cfgt_parse_msg(sip_msg_t *msg)
{
	int res = 1;
	if(parse_msg(msg->buf, msg->len, msg) != 0) {
		LM_ERR("outbuf buffer parsing failed!");
		return res;
	}

	if(msg->first_line.type == SIP_REQUEST) {
		if(!IS_SIP(msg)) {
			LM_DBG("non sip request message\n");
			goto done;
		}
	} else if(msg->first_line.type == SIP_REPLY) {
		if(!IS_SIP_REPLY(msg)) {
			LM_DBG("non sip reply message\n");
			goto done;
		}
	} else {
		LM_DBG("non sip message\n");
		goto done;
	}
	res = 0;

done:
	free_sip_msg(msg);
	return res;
}

int cfgt_process_route(struct sip_msg *msg, struct action *a)
{
	str routename;
	int ret = -1;
	int indx = 0;
	str flowname = STR_NULL;
	if(!_cfgt_node) {
		LM_ERR("node empty\n");
		return -1;
	}
	if(a->rname == NULL) {
		LM_DBG("no routename. type:%d\n", a->type);
		return 0;
	}
	LM_DBG("route from action:[%s]\n", a->rname);
	if(not_sip) {
		LM_DBG("not_sip flag set, not a SIP message, skip it\n");
		return 0;
	}
	routename.s = a->rname;
	routename.len = strlen(a->rname);
	switch(_cfgt_add_routename(_cfgt_node, a, &routename)) {
		case 2: /* same name */
			return 0;
		case 1: /* initial */
			LM_DBG("Initial route[%.*s]. dump vars\n", _cfgt_node->route->s.len,
					_cfgt_node->route->s.s);
			if(_cfgt_node_get_flowname(_cfgt_node->route, &indx, &flowname)
					< 0) {
				LM_ERR("cannot create flowname\n");
				return -1;
			}
			ret = _cfgt_set_dump(msg, _cfgt_node, &flowname);
			break;
		case 0: /* new */
			LM_DBG("Change from[%.*s] route to route[%.*s]. dump vars\n",
					_cfgt_node->route->prev->s.len,
					_cfgt_node->route->prev->s.s, _cfgt_node->route->s.len,
					_cfgt_node->route->s.s);
			if(_cfgt_node_get_flowname(_cfgt_node->route, &indx, &flowname)
					< 0) {
				LM_ERR("cannot create flowname\n");
				return -1;
			}
			ret = _cfgt_set_dump(msg, _cfgt_node, &flowname);
			break;
		case 3: /* back to previous */
			if(_cfgt_node_get_flowname(_cfgt_node->route, 0, &flowname) < 0) {
				LM_ERR("cannot create flowname\n");
				return -1;
			}
			ret = _cfgt_set_dump(msg, _cfgt_node, &flowname);
			_cfgt_del_routename(_cfgt_node);
			break;
		default:
			return -1;
	}
	if(flowname.s)
		pkg_free(flowname.s);
	return ret;
}

/*
TODO:
- parse for header cfgtest
*/
int cfgt_msgin(sr_event_param_t *evp)
{
	srjson_t *jobj;
	sip_msg_t msg;
	str *buf = (str *)evp->data;

	if(buf == NULL)
		return 0;
	LM_DBG("msg in:{%.*s}\n", buf->len, buf->s);

	// Check if it is a SIP message
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = buf->s;
	msg.len = buf->len;
	if(_cfgt_parse_msg(&msg) != 0) {
		LM_DBG("set 'not_sip' flag\n");
		not_sip = 1;
	} else {
		not_sip = 0;
	}

	if(_cfgt_node) {
		cfgt_save_node(_cfgt_node);
		_cfgt_remove_node(_cfgt_node);
		LM_DBG("node removed\n");
		_cfgt_node = NULL;
	}
	_cfgt_node = cfgt_create_node(NULL);
	if(_cfgt_node) {
		jobj = srjson_CreateStr(&_cfgt_node->jdoc, buf->s, buf->len);
		if(jobj == NULL) {
			LM_ERR("cannot create json object\n");
			return -1;
		}
		srjson_AddItemToArray(&_cfgt_node->jdoc, _cfgt_node->in, jobj);
		return 0;
	}
	LM_ERR("_cfgt_node empty\n");
	return -1;
}

int cfgt_pre(struct sip_msg *msg, unsigned int flags, void *bar)
{
	str unknown = {"unknown", 7};
	int get_hdr_result = 0, res;

	if(_cfgt_node) {
		if(_cfgt_node->msgid == 0) {
			LM_DBG("new node\n");
			get_hdr_result = _cfgt_get_hdr(msg, &_cfgt_node->uuid);
			if(get_hdr_result != 0 || _cfgt_node->uuid.len == 0) {
				if(not_sip) {
					LM_DBG("not_sip flag set, not a SIP message."
						   " Using 'unknown' uuid\n");
				} else if(get_hdr_result == 2) {
					LM_DBG("message not related to the cfgtest scenario."
						   " Using 'unknown' uuid\n");
				} else {
					LM_ERR("cannot get value of cfgtest uuid header."
						   " Using 'unknown' uuid\n");
				}
				pkg_str_dup(&_cfgt_node->uuid, &unknown);
			}
			res = _cfgt_get_uuid_id(_cfgt_node);
			LM_INFO("*** node uuid:[%.*s] id:[%d] created ***\n",
					STR_FMT(&_cfgt_node->uuid), _cfgt_node->msgid);
			return res;
		} else {
			LM_DBG("_cfgt_node->uuid:[%.*s]\n", _cfgt_node->uuid.len,
					_cfgt_node->uuid.s);
			if(_cfgt_cmp_hdr(msg, &_cfgt_node->uuid)) {
				LM_DBG("same uuid\n");
				return 1;
			} else {
				LM_DBG("different uuid\n");
			}
		}
	} else {
		LM_ERR("node empty??\n");
	}
	_cfgt_node = cfgt_create_node(msg);
	if(_cfgt_node) {
		LM_DBG("node created\n");
		return 1;
	}
	return -1;
}
int cfgt_post(struct sip_msg *msg, unsigned int flags, void *bar)
{
	str flowname = STR_NULL;

	if(_cfgt_node) {
		LM_DBG("dump last flow\n");
		if(_cfgt_node->route == NULL
				&& strncmp(_cfgt_node->uuid.s, "unknown", 7) == 0)
			LM_DBG("route is NULL and message doesn't belong to cfgtest "
				   "scenario\n");
		else if(_cfgt_node_get_flowname(_cfgt_node->route, 0, &flowname) < 0)
			LM_ERR("cannot create flowname\n");
		else
			_cfgt_set_dump(msg, _cfgt_node, &flowname);
		if(flowname.s)
			pkg_free(flowname.s);
		cfgt_save_node(_cfgt_node);
	}
	return 1;
}

int cfgt_msgout(sr_event_param_t *evp)
{
	srjson_t *jobj;
	sip_msg_t msg;
	str *buf = (str *)evp->data;

	if(buf == NULL)
		return 0;
	LM_DBG("msg out:{%.*s}\n", buf->len, buf->s);

	// Check if it is a SIP message
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = buf->s;
	msg.len = buf->len;
	if(_cfgt_parse_msg(&msg) != 0) {
		LM_DBG("set 'not_sip' flag\n");
		not_sip = 1;
	} else {
		not_sip = 0;
	}

	if(_cfgt_node) {
		jobj = srjson_CreateStr(&_cfgt_node->jdoc, buf->s, buf->len);
		if(jobj == NULL) {
			LM_ERR("cannot create json object\n");
			return -1;
		}
		srjson_AddItemToArray(&_cfgt_node->jdoc, _cfgt_node->out, jobj);
		return 0;
	}

	// Skip OPTION messages internally generated
	if(buf->len > 7 && strcasestr(buf->s, "OPTIONS")) {
		LM_DBG("OPTIONS message internally generated, skip it\n");
		return 0;
	}

	LM_ERR("node empty\n");
	return -1;
}

int _cfgt_clean(str *scen)
{
	if(strncmp(scen->s, "all", 3) == 0) {
		return _cfgt_remove_uuid(NULL, 1);
	}
	return _cfgt_remove_uuid(scen, 1);
}

int _cfgt_list_uuids(rpc_t *rpc, void *ctx)
{
	void *vh;
	struct str_hash_head *head;
	struct str_hash_entry *entry, *back;
	int i;

	if(_cfgt_uuid == NULL) {
		LM_DBG("no _cfgt_uuid\n");
		rpc->fault(ctx, 500, "Server error");
		return -1;
	}


	lock_get(&_cfgt_uuid->lock);
	for(i = 0; i < CFGT_HASH_SIZE; i++) {
		head = _cfgt_uuid->hash.table + i;
		clist_foreach_safe(head, entry, back, next)
		{
			if(rpc->add(ctx, "{", &vh) < 0) {
				rpc->fault(ctx, 500, "Server error");
				return -1;
			}
			rpc->struct_add(vh, "Sd", "uuid", &entry->key, "msgid", entry->u.n);
		}
	}
	lock_release(&_cfgt_uuid->lock);
	return 0;
}

/**
 *
 */
static const char *cfgt_rpc_mask_doc[2] = {"Specify module mask", 0};
static const char *cfgt_rpc_list_doc[2] = {
		"List scenarios currently in memory", 0};
static const char *cfgt_rpc_clean_doc[2] = {
		"Clean scenario, 'all' to clean all scenarios in memory", 0};


static void cfgt_rpc_mask(rpc_t *rpc, void *ctx)
{
	int mask = CFGT_DP_ALL;

	if(rpc->scan(ctx, "*d", &mask) != 1) {
		rpc->fault(ctx, 500, "invalid parameters");
		return;
	}
	cfgt_mask = mask;
	rpc->add(ctx, "s", "200 ok");
}

static void cfgt_rpc_list(rpc_t *rpc, void *ctx)
{
	if(_cfgt_list_uuids(rpc, ctx) >= 0)
		rpc->add(ctx, "s", "200 ok");
}

static void cfgt_rpc_clean(rpc_t *rpc, void *ctx)
{
	str scen = STR_NULL;

	if(rpc->scan(ctx, "S", &scen) != 1) {
		rpc->fault(ctx, 500, "invalid parameters");
		return;
	}
	if(_cfgt_clean(&scen) != 0) {
		rpc->fault(ctx, 500, "error in clean");
		return;
	}
	rpc->add(ctx, "s", "200 ok");
}

/* clang-format off */
rpc_export_t cfgt_rpc[] = {
		{"cfgt.mask", cfgt_rpc_mask, cfgt_rpc_mask_doc, 0},
		{"cfgt.list", cfgt_rpc_list, cfgt_rpc_list_doc, 0},
		{"cfgt.clean", cfgt_rpc_clean, cfgt_rpc_clean_doc, 0},
		{0, 0, 0, 0}
};
/* clang-format on */

int cfgt_init(void)
{
	if(rpc_register_array(cfgt_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	_cfgt_uuid = shm_malloc(sizeof(cfgt_hash_t));
	if(_cfgt_uuid == NULL) {
		SHM_MEM_ERROR;
		return -1;
	}
	if(!lock_init(&_cfgt_uuid->lock)) {
		LM_ERR("cannot init the lock\n");
		shm_free(_cfgt_uuid);
		_cfgt_uuid = NULL;
		return -1;
	}
	if(_cfgt_init_hashtable(&_cfgt_uuid->hash) < 0)
		return -1;
	sr_event_register_cb(SREV_NET_DATA_IN, cfgt_msgin);
	sr_event_register_cb(SREV_NET_DATA_OUT, cfgt_msgout);
	return 0;
}
