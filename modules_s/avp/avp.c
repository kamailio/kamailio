/*
 * $Id$
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  2005-03-28  avp_destination & xlset_destination - handle both nameaddr & uri texts (mma)
 *  2005-12-22  merge changes from private branch (mma)
 *  2006-01-03  avp_body merged (mma)
 */

#include <string.h>
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../error.h"
#include "../../lump_struct.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_nameaddr.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../trim.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../re.h"

#include "../../parser/parse_hname2.h"
#include "../xlog/xl_lib.h"
#define NO_SCRIPT -1

MODULE_VERSION

/* name of attributed used to store flags with command flags2attr */
#define HDR_ID 0
#define HDR_STR 1

#define PARAM_DELIM '/'
#define VAL_TYPE_INT (1<<0)
#define VAL_TYPE_STR (1<<1)

struct hdr_name {
	enum {hdrId, hdrStr} kind;
	union {
		int n;
		str s;
	} name;
	char field_delimiter;
	char array_delimiter;
	int val_types;
};

static int xlbuf_size=256;
static char* xlbuf=NULL;
str* xl_nul=NULL;
xl_print_log_f* xl_print=NULL;
xl_parse_format_f* xl_parse=NULL;
xl_get_nulstr_f* xl_getnul=NULL;

static int set_iattr(struct sip_msg*, char*, char*);
static int set_sattr(struct sip_msg*, char*, char*);
static int print_sattr(struct sip_msg*, char*, char*);
static int uri2attr(struct sip_msg*, char*, char*);
static int attr2uri(struct sip_msg*, char*, char*);
static int is_sattr_set( struct sip_msg*, char*, char*);
static int flags2attr(struct sip_msg*, char*, char*);
static int avp_equals (struct sip_msg*, char*, char*);
static int avp_equals_xl (struct sip_msg*, char*, char*);
static int avp_exists (struct sip_msg*, char*, char*);
static int dump_avp(struct sip_msg*, char*, char*);
static int xlset_attr (struct sip_msg*, char*, char*);
static int insert_req(struct sip_msg*, char*, char*);
static int append_req(struct sip_msg*, char*, char*);
static int replace_req(struct sip_msg*, char*, char*);
static int append_reply(struct sip_msg*, char*, char*);
static int avp_destination(struct sip_msg*, char*, char*);
static int xlset_destination(struct sip_msg*, char*, char*);
static int avp_subst(struct sip_msg*, char*, char*);
static int iattr_fixup(void** param, int param_no);
static int avp_hdr_body2attrs(struct sip_msg*, char*, char*);
static int avp_hdr_body2attrs_fixup(void**, int);
static int avp_hdr_body2attrs2_fixup(void**, int);
static int avp_delete(struct sip_msg*, char*, char*);


static int fixup_attr_1(void** param, int param_no); /* (attr_ident_t*) */
static int fixup_xl_1(void** param, int param_no); /* (xl_format*) */
//static int fixup_attr_1_str_2(void** param, int param_no); /* (attr_ident_t*, str*) */
static int fixup_str_1_attr_2(void** param, int param_no); /* (str*, attr_ident_t*) */
static int fixup_attr_1_xl_2(void** param, int param_no); /* (attr_ident_t*, xl_format*) */
static int fixup_attr_1_subst_2(void** param, int param_no); /* (attr_ident_t*, struct subst_expr*) */

static int avp_hdr_body2attrs2(struct sip_msg* m, char* header_, char* prefix_) {
	return avp_hdr_body2attrs(m, header_, prefix_);   // unless defined in cmds then fixup is not called, bug in module registration???
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"set_iattr",       set_iattr,    2, iattr_fixup, REQUEST_ROUTE | FAILURE_ROUTE},
	{"flags2attr",      flags2attr,   0, 0,           REQUEST_ROUTE | FAILURE_ROUTE},
	{"set_sattr",       set_sattr,    2, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"uri2attr",        uri2attr,     1, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"print_sattr",     print_sattr,  1, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"attr2uri",        attr2uri,     1, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"is_sattr_set",    is_sattr_set, 1, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_equals",      avp_equals,   2, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_equals_xl",   avp_equals_xl,2, fixup_attr_1_xl_2,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_exists",      avp_exists,   1, fixup_str_12,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"dump_avp",	    dump_avp,     0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
	{"xlset_attr",      xlset_attr,   2, fixup_attr_1_xl_2,    REQUEST_ROUTE | FAILURE_ROUTE},
	{"insert_avp_hf",   insert_req,   2, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"insert_avp_hf",   insert_req,   1, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"append_avp_hf",   append_req,   2, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"append_avp_hf",   append_req,   1, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"replace_avp_hf",  replace_req,  2, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"replace_avp_hf",  replace_req,  1, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_to_reply",    append_reply, 2, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_to_reply",    append_reply,     1, fixup_str_1_attr_2,   REQUEST_ROUTE | FAILURE_ROUTE},
	{"avp_destination", avp_destination,  1, fixup_attr_1,   REQUEST_ROUTE}, 
	{"xlset_destination",  xlset_destination,   1, fixup_xl_1,   REQUEST_ROUTE},
	{"avp_subst",       avp_subst,    2, fixup_attr_1_subst_2,    REQUEST_ROUTE | FAILURE_ROUTE},
	{"hdr_body2attrs",  avp_hdr_body2attrs, 2, avp_hdr_body2attrs_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
	{"hdr_body2attrs2", avp_hdr_body2attrs2, 2, avp_hdr_body2attrs2_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
	{"avp_delete",      avp_delete,   1, fixup_attr_1,   REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE}, 
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"xlbuf_size", PARAM_INT, &xlbuf_size},
	{0, 0, 0}
};


struct module_exports exports = {
	"avp",
	cmds,           /* Exported commands */
	0,		/* RPC */
	params,         /* Exported parameters */
	0,       	/* module initialization function */
	0,              /* response function*/
	0,              /* destroy function */
	0,              /* oncancel function */
	0               /* per-child init function */
};

static int xl_printstr(struct sip_msg* msg, xl_elog_t* format, char** res, int* res_len)
{
	int len;

	if (!format || !res) {
		LOG(L_ERR, "xl_printstr: Called with null format or res\n");
		return -1;
	}

	if (!xlbuf) {
		xlbuf=pkg_malloc((xlbuf_size+1)*sizeof(char));
		if (!xlbuf) {
			LOG(L_CRIT, "xl_printstr: No memory left for format buffer\n");
			return -1;
		}
	}

	len=xlbuf_size;
	if (xl_print(msg, format, xlbuf, &len)<0) {
		LOG(L_ERR, "xl_printstr: Error while formating result\n");
		return -1;
	}

	if ((xl_nul) && (xl_nul->len==len) && !strncmp(xl_nul->s, xlbuf, len))
		return 0;

	*res=xlbuf;
	if (res_len) *res_len=len;
	return len;
}


static int set_sattr(struct sip_msg* msg, char* attr, char* val)
{
	int_str name, value;

	WARN("set_sattr:Use of this method is deprecated, use $attr syntax for direct access\n");

	name.s = *(str*)attr;
	value.s = *(str*)val;

	if (add_avp(AVP_NAME_STR | AVP_VAL_STR, name, value) !=0 ) {
		LOG(L_ERR, "set_sattr: add_avp failed\n");
		return -1;
	}

	DBG("set_sattr ok\n");
	return 1;
}


static int set_iattr(struct sip_msg* msg, char* attr, char* nr)
{
	int_str name, value;

	WARN("set_iattr:Use of this method is deprecated, use $attr syntax for direct access\n");

	value.n = (int)(long)nr;
	name.s = *(str*)attr;

	if (add_avp(AVP_NAME_STR, name, value) != 0) {
		LOG(L_ERR, "set_iattr: add_avp failed\n");
		return -1;
	}

	LOG(L_DBG, "set_iattr ok\n");
	return 1;
}


static int flags2attr(struct sip_msg* msg, char* foo, char* bar)
{
	str s_name;
	int_str name, value;

	s_name.s = AVP_FLAGS;
	s_name.len = strlen(AVP_FLAGS);

	name.s = s_name;
	value.n = msg->flags;

	if (add_avp(AVP_NAME_STR, name, value) != 0) {
		LOG(L_ERR, "flags2attr: add_avp failed\n");
		return -1;
	}

	DBG("flags2attr ok\n");
	return 1;
}


static int print_sattr(struct sip_msg* msg, char* attr, char* s2)
{
	str s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	name.s = *(str*)attr;

	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR, name, &value, NULL);
	if (avp_entry == 0) {
		LOG(L_ERR, "print_sattr: AVP '%.*s' not found\n", name.s.len, ZSW(name.s.s));
		return -1;
	}

	s_value.s = value.s.s;
	s_value.len = value.s.len;
	LOG(L_INFO, "AVP: '%.*s'='%.*s'\n",
	    ((str*)attr)->len, ZSW(((str*)attr)->s), s_value.len, ZSW(s_value.s));
	return 1;
}


static int uri2attr(struct sip_msg* msg, char* attr, char* foo)
{
	str uri;

	get_request_uri(msg, &uri);
	return set_sattr(msg, attr, (char *)&uri);
}


static int is_sattr_set(struct sip_msg* msg, char* attr, char* foo)
{
	int_str name, value;
	struct usr_avp* avp_entry;

	WARN("set_iattr:Use of this method is deprecated, use $attr syntax for direct access\n");

	name.s = *(str*)attr;
	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				     name, &value, NULL);
	if (avp_entry == 0) {
		return -1;
	}

	return 1;
}


static int attr2uri(struct sip_msg* msg, char* attr, char* foo)
{
	str s_value;
	int_str name, value;
	struct usr_avp *avp_entry;

	name.s=*(str*)attr;

	avp_entry = search_first_avp(AVP_NAME_STR | AVP_VAL_STR,
				     name, &value, NULL);
	if (avp_entry == 0) {
		LOG(L_ERR, "attr2uri: AVP '%.*s' not found\n", name.s.len, ZSW(name.s.s));
		return -1;
	}

	s_value.s = value.s.s;
	s_value.len = value.s.len;
	if (rewrite_uri(msg, &s_value) < 0) {
		LOG(L_ERR, "attr2uri: no attribute found\n");
		return -1;
	}
	return 1;
}


/*
 *  returns 1 if msg contains an AVP with the given name and value,
 *  returns -1 otherwise
 */
static int avp_equals(struct sip_msg* msg, char* key, char* value)
{
	int_str avp_key, avp_value;
	struct usr_avp *avp_entry;
	struct search_state st;
	str* val_str, *key_str;

	WARN("avp_equals:Use of this method is deprecated, use $attr syntax for direct access\n");

	key_str = (str*)key;	
	
	avp_key.s = *key_str;
	avp_entry = search_first_avp(AVP_NAME_STR, avp_key, &avp_value, &st);

	if (avp_entry == 0) {
		DBG("avp_equals: AVP '%.*s' not found\n", key_str->len, ZSW(key_str->s));
		return -1;
	}

	if (!value) {
		DBG("avp_equals: at least one AVP '%.*s' found\n", key_str->len, ZSW(key_str->s));
		return 1;
	}

	val_str = (str*)value;
	while (avp_entry != 0) {
		if (avp_entry->flags & AVP_VAL_STR) {
			if ((avp_value.s.len == val_str->len) &&
			    !memcmp(avp_value.s.s, val_str->s, avp_value.s.len)) {
				DBG("avp_equals str ('%.*s', '%.*s'): TRUE\n",
				    key_str->len, ZSW(key_str->s),
				    val_str->len, ZSW(val_str->s));
				return 1;
			}
		} else {
			if (avp_value.n == str2s(val_str->s, val_str->len, 0)){
				DBG("avp_equals (%.*s, %.*s): TRUE\n",
				    key_str->len, ZSW(key_str->s),
				    val_str->len, ZSW(val_str->s));
				return 1;
			}
		}
		avp_entry = search_next_avp (&st, &avp_value);
	}

	DBG("avp_equals ('%.*s', '%.*s'): FALSE\n",
	    key_str->len, ZSW(key_str->s),
	    val_str->len, ZSW(val_str->s));
	return -1;
}

static int avp_equals_xl(struct sip_msg* m, char* name, char* format)
{
	avp_value_t avp_val;
	struct search_state st;
	str xl_val;
	avp_t* avp;
	
	if (xl_printstr(m, (xl_elog_t*) format, &xl_val.s, &xl_val.len)>0) {
		for (avp=search_avp(*(avp_ident_t*)name, &avp_val, &st);avp;avp=search_next_avp(&st, &avp_val)) {
			if (avp->flags & AVP_VAL_STR) {
				if ((avp_val.s.len == xl_val.len) &&
				    !memcmp(avp_val.s.s, xl_val.s, avp_val.s.len)) return 1;
			} else {
				if (avp_val.n == str2s(xl_val.s, xl_val.len, 0)) return 1;
			}
		}
		return -1;
	}

	ERR("avp_equals_xl:Error while expanding xl_format\n");
	return -1;
}

static int avp_exists (struct sip_msg* m, char* key, char* value)
{
	WARN("avp_exists:Use of this method is deprecated, use $attr syntax for direct access\n");

	return avp_equals(m, key, NULL);
}

/*
 * sends avp list to log in readable form
 *
 */

static void dump_avp_reverse(avp_t* avp)
{
	str* name;
	int_str val;

	if (avp) {
		/* AVPs are added to front of the list, reverse by recursion */
		dump_avp_reverse(avp->next);

		name=get_avp_name(avp);
		get_avp_val(avp, &val);
		switch ( avp->flags&(AVP_NAME_STR|AVP_VAL_STR) )
		{
			case 0:
				/* avp type ID, int value */
				LOG(L_INFO,"AVP[%d]=%d\n", avp->id, val.n);
				break;
			case AVP_NAME_STR:
				/* avp type str, int value */
				name=get_avp_name(avp);
				LOG(L_INFO,"AVP[\"%.*s\"]=%d\n", name->len, name->s, val.n);
				break;
			case AVP_VAL_STR:
				/* avp type ID, str value */
				LOG(L_INFO,"AVP[%d]=\"%.*s\"\n", avp->id, val.s.len, val.s.s);
				break;
			case AVP_NAME_STR|AVP_VAL_STR:
				/* avp type str, str value */
				name=get_avp_name(avp);
				LOG(L_INFO,"AVP[\"%.*s\"]=\"%.*s\"\n", name->len, name->s, val.s.len, val.s.s);
				break;
		}
	}
}

static int dump_avp(struct sip_msg* m, char* x, char* y)
{
	avp_list_t avp_list;

	avp_list=get_avp_list(AVP_CLASS_GLOBAL);
	INFO("class=GLOBAL\n");
	if (!avp_list) {
		LOG(L_INFO,"INFO: No AVP present\n");
	} else {
		dump_avp_reverse(avp_list);
	}
	avp_list=get_avp_list(AVP_CLASS_DOMAIN | AVP_TRACK_FROM);
	INFO("track=FROM class=DOMAIN\n");
	if (!avp_list) {
		LOG(L_INFO,"INFO: No AVP present\n");
	} else {
		dump_avp_reverse(avp_list);
	}
	avp_list=get_avp_list(AVP_CLASS_DOMAIN | AVP_TRACK_TO);
	INFO("track=TO class=DOMAIN\n");
	if (!avp_list) {
		LOG(L_INFO,"INFO: No AVP present\n");
	} else {
		dump_avp_reverse(avp_list);
	}
	avp_list=get_avp_list(AVP_CLASS_USER | AVP_TRACK_FROM);
	INFO("track=FROM class=USER\n");
	if (!avp_list) {
		LOG(L_INFO,"INFO: No AVP present\n");
	} else {
		dump_avp_reverse(avp_list);
	}
	avp_list=get_avp_list(AVP_CLASS_USER | AVP_TRACK_TO);
	INFO("track=TO class=USER\n");
	if (!avp_list) {
		LOG(L_INFO,"INFO: No AVP present\n");
	} else {
		dump_avp_reverse(avp_list);
	}
	return 1;
}

static int xlset_attr(struct sip_msg* m, char* name, char* format)
{
	avp_value_t val;
	
	if (xl_printstr(m, (xl_elog_t*) format, &val.s.s, &val.s.len)>0) {
		if (add_avp(((avp_ident_t*)name)->flags | AVP_VAL_STR, ((avp_ident_t*)name)->name, val)) {
			ERR("xlset_attr:Error adding new AVP\n");
			return -1;
		}
		return 1;
	}
	
	ERR("xlset_attr:Error while expanding xl_format\n");
	return -1;
}

static int request_hf_helper(struct sip_msg* msg, str* hf, avp_ident_t* ident, struct lump* anchor, struct search_state* st, int front, int reverse, int reply)
{
        struct lump* new_anchor;
        static struct search_state state;
		struct usr_avp* avp;
        char* s;
        str fin_val;
        int len, ret;
        int_str val;
        struct hdr_field* pos;
        struct hdr_field* found=NULL;

        if (!anchor && !reply) {

                if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
                        LOG(L_ERR, "ERROR: request_hf_helper: Error while parsing message\n");
                        return -1;
                }

                pos = msg->headers;
                while (pos && (pos->type!=HDR_EOH_T)) {
                        if ((hf->len==pos->name.len)
                        && (!strncasecmp(hf->s, pos->name.s, pos->name.len))) {
                                found=pos;
                                if (front)
                                        break;
                        }
                        pos=pos->next;
                }

                if (found) {
                        if (front) {
                                len=found->name.s - msg->buf;
                        } else {
                                len=found->name.s + found->len - msg->buf;
                        }
                } else {
                        len = msg->unparsed - msg->buf;
                }

                new_anchor = anchor_lump(msg, len, 0, 0);
                if (new_anchor == 0) {
                        LOG(L_ERR, "ERROR: request_hf_helper: Can't get anchor\n");
                        return -1;
                }

        } else {
        	new_anchor=anchor;
        }

	if (!st) {
		st=&state;
		avp=search_avp(*ident, NULL, st);
		ret=-1;
	} else {
		avp=search_next_avp(st, NULL);
		ret=1;
	}
	
	if (avp) {	
		if (reverse && (request_hf_helper(msg, hf, ident, new_anchor, st, front, reverse, reply)==-1)) {
			return -1;
		}

		get_avp_val(avp, &val);
		if (avp->flags & AVP_VAL_STR) {
			fin_val=val.s;
		} else {
			fin_val.s=int2str(val.n, &fin_val.len);
		}
		
		len=hf->len+2+fin_val.len+2;
		s = (char*)pkg_malloc(len);
		if (!s) {
			LOG(L_ERR, "ERROR: request_hf_helper: No memory left for data lump\n");
			return -1;
		}

		memcpy(s, hf->s, hf->len);
		memcpy(s+hf->len, ": ", 2 );
		memcpy(s+hf->len+2, fin_val.s, fin_val.len );
		memcpy(s+hf->len+2+fin_val.len, "\r\n", 2);


		if (reply) {
			if (add_lump_rpl( msg, s, len, LUMP_RPL_HDR | LUMP_RPL_NODUP) == 0) {
				LOG(L_ERR,"ERROR: request_hf_helper: Can't insert RPL lump\n");
				pkg_free(s);
				return -1;
			}
		} else {
			if ((front && (insert_new_lump_before(new_anchor, s, len, 0) == 0))
			|| (!front && (insert_new_lump_after(new_anchor, s, len, 0) == 0))) {
				LOG(L_ERR, "ERROR: request_hf_helper: Can't insert lump\n");
				pkg_free(s);
				return -1;
			}
		}
		if (!reverse && (request_hf_helper(msg, hf, ident, new_anchor, st, front, reverse, reply)==-1)) {
			return -1;
		}
		return 1;
	};

	/* in case of topmost call (st==NULL) return error */
	/* otherwise it's OK, no more AVPs found */
	return ret; 
}

static int insert_req(struct sip_msg* m, char* hf, char* name)
{
	avp_ident_t ident, *avp;
	
	if (name) {
		avp=(avp_ident_t*)name;
	} else {
		ident.name.s=*(str*)hf;
		ident.flags=AVP_NAME_STR;
		ident.index=0;
		avp=&ident;
	}
	return (request_hf_helper(m, (str*)hf, avp, NULL, NULL, 1, 0, 0));
}

static int append_req(struct sip_msg* m, char* hf, char* name)
{
	avp_ident_t ident, *avp;
	
	if (name) {
		avp=(avp_ident_t*)name;
	} else {
		ident.name.s=*(str*)hf;
		ident.flags=AVP_NAME_STR;
		ident.index=0;
		avp=&ident;
	}
	return (request_hf_helper(m, (str*)hf, avp, NULL, NULL, 0, 1, 0));
}

static int replace_req(struct sip_msg* m, char* hf, char* name)
{
	struct hdr_field* pos;

	if (parse_headers(m, HDR_EOH_F, 0) == -1) {
		LOG(L_ERR, "ERROR: replace_req: Error while parsing message\n");
		return -1;
	}

	pos = m->headers;
	while (pos && (pos->type!=HDR_EOH_T)) {
		if ((((str*)hf)->len==pos->name.len)
		&& (!strncasecmp(((str*)hf)->s, pos->name.s, pos->name.len))) {
			if (del_lump(m, pos->name.s - m->buf, pos->len, 0)==0) {
				LOG(L_ERR,"ERROR: Can't insert del lump\n");
				return -1;
			}
		}
		pos=pos->next;
	}
	return append_req(m, hf, name);
}

static int append_reply(struct sip_msg* m, char* hf, char* name)
{
	avp_ident_t ident, *avp;
	
	if (name) {
		avp=(avp_ident_t*)name;
	} else {
		ident.name.s=*(str*)hf;
		ident.flags=AVP_NAME_STR;
		ident.index=0;
		avp=&ident;
	}
	return (request_hf_helper(m, (str*)hf, avp, NULL, NULL, 0, 1, 1));
}

static int w_set_destination(struct sip_msg* m, str* dest)
{
	name_addr_t nameaddr;

	if (!parse_nameaddr(dest, &nameaddr)) {
		return set_dst_uri(m, &nameaddr.uri);
	} else {
		/* it is just URI, pass it through */
		return set_dst_uri(m, dest);
	}
}

static int xlset_destination(struct sip_msg* m, char* format, char* x)
{
	str val;

	if (xl_printstr(m, (xl_elog_t*) format, &val.s, &val.len)>0) {
		DBG("Setting dest to: '%.*s'\n", val.len, val.s);
		if (w_set_destination(m, &val))
			return 1;
		else
			return -1;
	} else
		return -1;

}

static int avp_destination(struct sip_msg* m, char* avp_name, char* x)
{
	avp_t* avp;
	avp_value_t val;
	
	
	if ((avp=search_avp(*(avp_ident_t*)avp_name, &val, NULL))) {
		if (avp->flags & AVP_VAL_STR) {
			if (w_set_destination(m, &val.s)){
				LOG(L_ERR, "ERROR: avp_destination: Can't set dst uri\n");
				return -1;
			};
			return 1;
		} else {
			ERR("avp_destination:AVP has numeric value\n");
			return -1;
		}
	}
	return -1;
}

static int avp_subst(struct sip_msg* m, char* avp_name, char* subst)
{
	avp_t* avp;
	avp_value_t val;
	str *res = NULL;
	int count;

    if ((avp=search_avp(*(avp_ident_t*)avp_name, &val, NULL))) {
		if (avp->flags & AVP_VAL_STR) {
			res=subst_str(val.s.s, m, (struct subst_expr*)subst, &count);
			if (res == NULL) {
				ERR("avp_subst: error while running subst\n");
				goto error;
			}
			DBG("avp_subst: %d, result %.*s\n", count, res->len, ZSW(res->s));
			val.s = *res;
			if (add_avp_before(avp, ((avp_ident_t*)avp_name)->flags | AVP_VAL_STR, ((avp_ident_t*)avp_name)->name, val)) {
				ERR("avp_subst: error while adding new AVP\n");
				goto error;
			};
			destroy_avp(avp);
			return 1;
		} else {
			ERR("avp_subst: AVP has numeric value\n");
			goto error;
		}
	} else {
		ERR("avp_subst: AVP[%.*s] index %d, flags %x not found\n", ((avp_ident_t*)avp_name)->name.s.len, ((avp_ident_t*)avp_name)->name.s.s,
				((avp_ident_t*)avp_name)->index, ((avp_ident_t*)avp_name)->flags);
		goto error;
	}
error:
	if (res) pkg_free(res);
	return -1;
}

static int avp_delete(struct sip_msg* m, char* avp_name, char* x)
{
	avp_t* avp;
	struct search_state st;	
	
	avp=search_avp(*(avp_ident_t*)avp_name, 0, &st);
	while (avp) {
		destroy_avp(avp);
		avp = search_next_avp(&st, 0);
	}
	return 1;
}
			
static int avp_hdr_body2attrs(struct sip_msg* m, char* header_, char* prefix_)
{
	char name_buf[50];
	str *prefix = (str*) prefix_;
	struct hdr_name *header = (void*) header_;
	struct hdr_field *hf;
	str s, name, val;
	int_str name2, val2;
	int val_type, arr;
	if (header->kind == HDR_STR) {
		if (parse_headers(m, HDR_EOH_F, 0) == -1) {
			LOG(L_ERR, "ERROR: avr_hdr_body2attrs: Error while parsing message\n");
		        return -1;
		}

		for (hf=m->headers; hf; hf=hf->next) {
			if ( (header->name.s.len == hf->name.len)
				&& (!strncasecmp(header->name.s.s, hf->name.s, hf->name.len)) ) {
				break;
			}
		}
	}
	else {
		if (parse_headers(m, header->name.n, 0) == -1) {
			LOG(L_ERR, "ERROR: avr_hdr_body2attrs: Error while parsing message\n");
		        return -1;
		}
		switch (header->name.n) {
		//	HDR_xxx:
		default:
			hf = NULL;
			break;
		}
	}
	if (!hf || !hf->body.len)
		return 1;

	// parse body of hf
	s = hf->body;
	name_buf[0] = '\0';
	while (s.len) {
		trim_leading(&s);
		name.s = s.s;
		while ( s.len &&
		       ( (s.s[0] >= 'a' && s.s[0] <= 'z') ||
		         (s.s[0] >= 'A' && s.s[0] <= 'Z') ||
		         (s.s[0] >= '0' && s.s[0] <= '9') ||
		         s.s[0] == '_' || s.s[0] == '-'
		       ) ) {
			s.s++;
			s.len--;
		}
		if (s.s == name.s)
			break;
		name.len = s.s - name.s;
		trim_leading(&s);
		if (!s.len)
			break;
		if (s.s[0] == '=') {
			s.s++;
			s.len--;
			arr = -1;

			while (s.len) {
				trim_leading(&s);
				val_type = 0;
				if (!s.len)
					break;
				if (s.s[0] == '"') {
					s.s++;
					s.len--;
					val.s = s.s;

					s.s = q_memchr(s.s, '\"', s.len);
					if (!s.s)
						break;
					val.len = s.s - val.s;
					val_type = AVP_VAL_STR;
					s.s++;
					s.len -= s.s - val.s;
				}
				else {
					int r;
					val.s = s.s;
					if (s.s[0] == '+' || s.s[0] == '-') {
						s.s++;
						s.len--;
					}
					val2.n = 0; r = 0;
					while (s.len) {
						if (s.s[0] == header->field_delimiter || (header->array_delimiter && header->array_delimiter == s.s[0]))
							goto token_end;
						switch (s.s[0]) {
							case ' ':
							case '\t':
							case '\n':
							case '\r':
								goto token_end;
						}
						if (!val_type && s.s[0] >= '0' && s.s[0]<= '9') {
							r++;
							val2.n *= 10;
							val2.n += s.s[0] - '0';
							// overflow detection ???
						}
						else {
							val_type = AVP_VAL_STR;
						}
						s.s++;
						s.len--;
					}
				token_end:
					if (r == 0) val_type = AVP_VAL_STR;
					if (!val_type && val.s[0] == '-') {
						val2.n = -val2.n;
					}
					val.len = s.s - val.s;
				}
				trim_leading(&s);
				if (arr >= 0 || (s.len && header->array_delimiter && header->array_delimiter == s.s[0])) {
					arr++;
					if (arr == 100)
						LOG(L_ERR, "ERROR: avp index out of limit\n");
				}
				if (val.len && arr < 100) {
					if (prefix != NULL || arr >= 0) {
						if ((prefix?prefix->len:0)+name.len+1+((arr>=0)?3/*#99*/:0) > sizeof(name_buf)) {
							if (arr <= 0)
								LOG(L_ERR, "ERROR: avp name too long\n");
							goto cont;
						}
						name2.s.len = 0;
						name2.s.s = name_buf;
						if (prefix != NULL) {
							if (name_buf[0] == '\0') {
								memcpy(&name_buf[0], prefix->s, prefix->len);
							}
							name2.s.len += prefix->len;
						}
						if (arr <= 0) {
							memcpy(&name_buf[name2.s.len], name.s, name.len);
						}
						name2.s.len += name.len;
						if (arr >= 0) {
							name_buf[name2.s.len] = '#';
							name2.s.len++;
							if (arr >= 10) {
								name_buf[name2.s.len] = '0'+ (arr / 10);
								name2.s.len++;
							}
							name_buf[name2.s.len] = '0'+ (arr % 10);
							name2.s.len++;
						}
					}
					else {
						name2.s.s = name.s;
						name2.s.len = name.len;
					}
					if ( ((val_type & AVP_VAL_STR) && (header->val_types & VAL_TYPE_STR)) ||
					     ((val_type & AVP_VAL_STR) == 0 && (header->val_types & VAL_TYPE_INT))  ) {
						if (val_type) {
							val2.s.s = val.s;
							val2.s.len = val.len;
							DBG("DEBUG: avp_hdr_body2attrs: adding avp '%.*s', sval: '%.*s'\n", name2.s.len, (char*) name2.s.s, val.len, val.s);
						} else {
							DBG("DEBUG: avp_hdr_body2attrs: adding avp '%.*s', ival: '%d'\n", name2.s.len, (char*) name2.s.s, val2.n);
						}
						if ( add_avp(AVP_NAME_STR | val_type, name2, val2)!=0) {
							LOG(L_ERR, "ERROR: avp_hdr_body2attrs: add_avp failed\n");
							return 1;
						}
					}
				}
			cont:
				if (s.len && header->array_delimiter && header->array_delimiter == s.s[0]) {
					s.s++;
					s.len--;
				}
				else {
					break;
				}
			};
		}
		if (s.len && s.s[0] == header->field_delimiter) {
			s.s++;
			s.len--;
		}
		else {
			break;
		}
	}
	return 1;
}

static int avp_hdr_body2attrs_fixup(void** param, int param_no) {
	char *c, *params;
	struct hdr_name *h;
	int n;
	str *s;
	if (param_no == 1) {
		c = *param;
		if (*c == '#') {
			c++;
			n = strtol(c, &params, 10);
			switch (*params) {
				case PARAM_DELIM:
					break;
				case 0:
					params = 0;
					break;
				default:
					LOG(L_ERR, "avp_hdr_body2attrs_fixup: bad AVP value\n");
					return E_CFG;
			}
			switch (n) {
//				case HDR_xxx:
//				case HDR_xxx:
//					break;
				default:
					LOG(L_ERR, "avp_hdr_body2attrs_fixup: header name is not valid and supported HDR_xxx id '%s' resolved as %d\n", c, n);
					return E_CFG;
			}
			h = pkg_malloc(sizeof(*h));
			if (!h) {
				LOG(L_ERR, "avp_hdr_body2attrs_fixup: out of memory\n");
				return E_OUT_OF_MEM;
			}

			h->kind = HDR_ID;
			h->name.n = n;
			pkg_free(*param);

		}
		else {
			params = strchr(c, PARAM_DELIM);
			if (params)
				n = params-c;
			else
				n = strlen(c);
			if (n == 0) {
				LOG(L_ERR, "avp_hdr_body2attrs_fixup: header name is empty\n");
				return E_CFG;
			}
			h = pkg_malloc(sizeof(*h)+n+1);
			if (!h) {
				LOG(L_ERR, "avp_hdr_body2attrs_fixup: out of memory\n");
				return E_OUT_OF_MEM;
			}
			h->kind = HDR_STR;
			h->name.s.len = n;
			h->name.s.s = (char *) h + sizeof(*h);
			memcpy(h->name.s.s, c, n+1);
		}
		if (params) {
			h->val_types = 0;
			while (*params) {
				switch (*params) {
					case 'i':
					case 'I':
						h->val_types = VAL_TYPE_INT;
						break;
					case 's':
					case 'S':
						h->val_types = VAL_TYPE_STR;
						break;
					case PARAM_DELIM:
						break;
					default:
						LOG(L_ERR, "avp_hdr_body2attrs_fixup: bad field param modifier near '%s'\n", params);
						return E_CFG;
				}
				params++;
			}
			if (!h->val_types) {
				LOG(L_ERR, "avp_hdr_body2attrs_fixup: no field param modifier specified\n");
				return E_CFG;
			}
		}
		else {
			h->val_types = VAL_TYPE_INT|VAL_TYPE_STR;
		}
		pkg_free(*param);
		h->field_delimiter = ',';
		h->array_delimiter = '\0';

		*param = h;
	}
	else if (param_no == 2) {
		n = strlen(*param);
		if (n == 0) {
			s = NULL;
		}
		else {
			s = pkg_malloc(sizeof(*s)+n+1);
			if (!s) {
				LOG(L_ERR, "avp_hdr_body2attrs_fixup: out of memory\n");
				return E_OUT_OF_MEM;
			}
			s->len = n;
			s->s = (char *) s + sizeof(*s);
			memcpy(s->s, *param, n+1);
		}
		pkg_free(*param);
		*param = s;
	}
	return 0;
}

static int avp_hdr_body2attrs2_fixup(void** param, int param_no) {
	struct hdr_name *h;
	int res = avp_hdr_body2attrs_fixup(param, param_no);
	if (res == 0 && param_no == 1) {
		h = *param;
		h->field_delimiter = ';';
		h->array_delimiter = ',';
	}
	return res;
}

static int iattr_fixup(void** param, int param_no)
{
	unsigned long num;
	int err;

	str* s;

	if (param_no == 1) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "iattr_fixup: No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	} else if (param_no == 2) {
		num = str2s(*param, strlen(*param), &err);

		if (err == 0) {
			pkg_free(*param);
			*param=(void*)num;
		} else {
			LOG(L_ERR, "iattr_fixup: Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}


/*
 * Convert xl format string to xl format description
 */
static int fixup_xl_1(void** param, int param_no)
{
 	xl_elog_t* model;

	if (!xl_print) {
		xl_print=(xl_print_log_f*)find_export("xprint", NO_SCRIPT, 0);

		if (!xl_print) {
			LOG(L_CRIT,"ERROR: cannot find \"xprint\", is module xlog loaded?\n");
			return -1;
		}
	}

	if (!xl_parse) {
		xl_parse=(xl_parse_format_f*)find_export("xparse", NO_SCRIPT, 0);

		if (!xl_parse) {
			LOG(L_CRIT,"ERROR: cannot find \"xparse\", is module xlog loaded?\n");
			return -1;
		}
	}

	if (!xl_nul) {
		xl_getnul=(xl_get_nulstr_f*)find_export("xnulstr", NO_SCRIPT, 0);
		if (xl_getnul)
			xl_nul=xl_getnul();

		if (!xl_nul){
                        LOG(L_CRIT,"ERROR: cannot find \"xnulstr\", is module xlog loaded?\n");
                        return -1;
                }
                else
                 LOG(L_INFO,"INFO: xlog null is \"%.*s\"\n", xl_nul->len, xl_nul->s);

	}

	if (param_no == 1 ) {
                if(*param) {
                        if(xl_parse((char*)(*param), &model)<0) {
                                LOG(L_ERR, "ERROR: xl_fixup: wrong format[%s]\n", (char*)(*param));
                                return E_UNSPEC;
                        }
                        *param = (void*)model;
                        return 0;
                } else {
                        LOG(L_ERR, "ERROR: xl_fixup: null format\n");
                        return E_UNSPEC;
                }
	}

	return 0;
}

static int fixup_attr_1_xl_2(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_attr_1(param, 1);
	}
	
	if (param_no == 2) {
		return fixup_xl_1(param, 1);
	}
	
	return 0;
}

static int fixup_attr_1(void** param, int param_no)
{
	avp_ident_t *attr = NULL;
	str s;
	
	if (param_no != 1) return -1;
	
	if (!((attr=pkg_malloc(sizeof(avp_ident_t))))) {
		ERR("Not enough memory\n");
		return E_OUT_OF_MEM;
	}
	
	s.s = *param;
	s.len = strlen(s.s);

	if (parse_avp_ident (&s, attr)) {
		ERR("Error while parsing AVP identification\n");
		return -1;
	}

	DBG("fix_attr: @%p AVP[%.*s] index %d, flags %x\n", attr,
			attr->name.s.len, attr->name.s.s,
			attr->index, attr->flags);
	
	/* If NAME_STR then don't free *param
	 * as attr->name points to it
	 */
	if ((attr->flags & AVP_NAME_STR) == 0) pkg_free(*param);
	*param=(void*) attr;
	return 0;
}

static int fixup_str_1_attr_2(void** param, int param_no)
{
	if (param_no == 1) {
		return fixup_str_12(param, 1);
	}

	if (param_no == 2) {
		return fixup_attr_1(param, 1);
	}

	return 0;
}

static int fixup_attr_1_subst_2(void** param, int param_no)
{
	struct subst_expr* subst;
	str s;
	
	if (param_no == 1) {
		return fixup_attr_1(param, 1);
	}

	if (param_no == 2) {
		s.s = *param;
		s.len = strlen(s.s);
		subst = subst_parser(&s);
		if (!subst) {
			ERR("fixup_attr_1_subst_2: error while parsing subst\n");
			return -1;
		}
		pkg_free(*param);
		*param=(void*) subst;
	}

	return 0;
}

