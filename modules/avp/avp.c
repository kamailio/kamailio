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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  2005-03-28  avp_destination & xlset_destination - handle both nameaddr & uri texts (mma)
 *  2005-12-22  merge changes from private branch (mma)
 *  2006-01-03  avp_body merged (mma)
 */

#include <string.h>
#include <stdlib.h>
#ifdef EXTRA_DEBUG
#include <assert.h>
#endif
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
#include "../../action.h"

#include "../../parser/parse_hname2.h"
#include "../xprint/xp_lib.h"
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
static str* xl_nul=NULL;
static xl_print_log_f* xl_print=NULL;
static xl_parse_format_f* xl_parse=NULL;
static xl_elog_free_all_f* xl_free=NULL;
static xl_get_nulstr_f* xl_getnul=NULL;

static int mod_init();
static int set_iattr(struct sip_msg* msg, char* p1, char* p2);
static int set_sattr(struct sip_msg* msg, char* p1, char* p2);
static int print_attr(struct sip_msg* msg, char* p1, char* p2);
static int del_attr(struct sip_msg* msg, char* p1, char* p2);
static int subst_attr(struct sip_msg* msg, char* p1, char* p2);
static int flags2attr(struct sip_msg* msg, char* p1, char* p2);
static int attr2uri(struct sip_msg* msg, char* p1, char* p2);
static int dump_attrs(struct sip_msg* msg, char* p1, char* p2);
static int attr_equals(struct sip_msg* msg, char* p1, char* p2);
static int attr_exists(struct sip_msg* msg, char* p1, char* p2);
static int attr_equals_xl(struct sip_msg* msg, char* p1, char* p2);
static int xlset_attr(struct sip_msg* msg, char* p1, char* p2);
static int xlfix_attr(struct sip_msg* msg, char* p1, char* p2);
static int insert_req(struct sip_msg* msg, char* p1, char* p2);
static int append_req(struct sip_msg* msg, char* p1, char* p2);
static int replace_req(struct sip_msg* msg, char* p1, char* p2);
static int append_reply(struct sip_msg* msg, char* p1, char* p2);
static int attr_destination(struct sip_msg* msg, char* p1, char* p2);
static int xlset_destination(struct sip_msg* msg, char* p1, char* p2);
static int attr_hdr_body2attrs(struct sip_msg* msg, char* p1, char* p2);
static int attr_hdr_body2attrs2(struct sip_msg* msg, char* p1, char* p2);
static int del_attrs(struct sip_msg* msg, char* p1, char* p2);

static int set_iattr_fixup(void**, int);
static int avpid_fixup(void**, int);
static int subst_attr_fixup(void**, int);
static int fixup_part(void**, int);
static int fixup_xl_1(void**, int);
static int fixup_attr_1_xl_2(void**, int);
static int fixup_str_1_attr_2(void**, int);
static int xlfix_attr_fixup(void** param, int param_no);
static int attr_hdr_body2attrs_fixup(void**, int);
static int attr_hdr_body2attrs2_fixup(void**, int);
static int avpgroup_fixup(void**, int);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
    {"set_iattr",         set_iattr,            2, set_iattr_fixup,            REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"set_sattr",         set_sattr,            2, fixup_var_str_12,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"set_attr",          set_sattr,            2, fixup_var_str_12,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"print_attr",        print_attr,           1, avpid_fixup,                REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"del_attr",          del_attr,             1, avpid_fixup,                REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE}, 
    {"del_attrs",         del_attrs,            1, avpgroup_fixup,             REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE}, 
    {"subst_attr",        subst_attr,           2, subst_attr_fixup,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"flags2attr",        flags2attr,           1, avpid_fixup,                REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"attr2uri",          attr2uri,             1, fixup_part,                 REQUEST_ROUTE | FAILURE_ROUTE},
    {"attr2uri",          attr2uri,             2, fixup_part,                 REQUEST_ROUTE | FAILURE_ROUTE},
    {"dump_attrs",	  dump_attrs,           0, 0,                          REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"dump_attrs",	  dump_attrs,           1, avpgroup_fixup,             REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"attr_equals",       attr_equals,          2, fixup_var_str_12,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"attr_exists",       attr_exists,          1 , fixup_var_str_1,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"attr_equals_xl",    attr_equals_xl,       2, fixup_attr_1_xl_2,          REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"xlset_attr",        xlset_attr,           2, fixup_attr_1_xl_2,          REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"xlfix_attr",        xlfix_attr,           1, xlfix_attr_fixup,           REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE}, 
    {"insert_attr_hf",    insert_req,           2, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"insert_attr_hf",    insert_req,           1, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"append_attr_hf",    append_req,           2, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"append_attr_hf",    append_req,           1, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"replace_attr_hf",   replace_req,          2, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"replace_attr_hf",   replace_req,          1, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"attr_to_reply",     append_reply,         2, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"attr_to_reply",     append_reply,         1, fixup_str_1_attr_2,         REQUEST_ROUTE | FAILURE_ROUTE},
    {"attr_destination",  attr_destination,     1, avpid_fixup,                REQUEST_ROUTE | FAILURE_ROUTE}, 
    {"xlset_destination", xlset_destination,    1, fixup_xl_1,                 REQUEST_ROUTE},
    {"hdr_body2attrs",    attr_hdr_body2attrs,  2, attr_hdr_body2attrs_fixup,  REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
    {"hdr_body2attrs2",   attr_hdr_body2attrs2, 2, attr_hdr_body2attrs2_fixup, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE},
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
    cmds,       /* Exported commands */
    0,          /* RPC */
    params,     /* Exported parameters */
    mod_init,          /* module initialization function */
    0,          /* response function*/
    0,          /* destroy function */
    0,          /* oncancel function */
    0           /* per-child init function */
};


static int set_iattr_fixup(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_var_str_12(param, param_no);
    } else {
	return fixup_var_int_12(param, param_no);
    }
}


static int get_avp_id(avp_ident_t* id, fparam_t* p, struct sip_msg* msg)
{
    str str_id;
    avp_t* avp;
    avp_value_t val;
    int ret;

    switch(p->type) {
    case FPARAM_AVP:
	avp = search_avp(p->v.avp, &val, 0);
	if (!avp) {
	    DBG("get_avp_id: AVP %s does not exist\n", p->orig);
	    return -1;
	}
	if ((avp->flags & AVP_VAL_STR) == 0) {
	    DBG("get_avp_id: Not a string AVP\n");
	    return -1;
	}
	str_id = val.s;
	break;

    case FPARAM_SELECT:
	ret = run_select(&str_id, p->v.select, msg);
	if (ret < 0 || ret > 0) return -1;
	break;

	case FPARAM_STR:
	str_id = p->v.str;
	break;

    default:
	ERR("Invalid parameter type in get_avp_id\n");
	return -1;
    }

    return parse_avp_ident(&str_id, id);
}


static int set_iattr(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t avpid;
    int_str value;

    if (get_avp_id(&avpid, (fparam_t*)p1, msg) < 0) {
	return -1;
    }
    
    if (get_int_fparam(&value.n, msg, (fparam_t*)p2) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
    if (add_avp(avpid.flags | AVP_NAME_STR, avpid.name, value) != 0) {
	ERR("add_avp failed\n");
	return -1;
    }
    return 1;
}


static int set_sattr(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t avpid;
    int_str value;
    
    if (get_avp_id(&avpid, (fparam_t*)p1, msg) < 0) {
	return -1;
    }

    if (get_str_fparam(&value.s, msg, (fparam_t*)p2) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p2)->orig);
	return -1;
    }
    
    if (add_avp(avpid.flags | AVP_NAME_STR | AVP_VAL_STR, avpid.name, value) != 0) {
	ERR("add_avp failed\n");
	return -1;
    }
    
    return 1;
}


static int avpid_fixup(void** param, int param_no)
{
    if (param_no == 1) {
		if (fix_param(FPARAM_AVP, param) == 0) return 0;
		ERR("Invalid AVP identifier: '%s'\n", (char*)*param);
		return -1;
    }
    return 0;
}


static int print_attr(struct sip_msg* msg, char* p1, char* p2)
{
    fparam_t* fp;
    int_str value;
    avp_t *avp;

    fp = (fparam_t*)p1;
    
    avp = search_avp(fp->v.avp, &value, NULL);
    if (avp == 0) {
	LOG(L_INFO, "AVP '%s' not found\n", fp->orig);
	return -1;
    }
    
    if (avp->flags & AVP_VAL_STR) {
	LOG(L_INFO, "AVP: '%s'='%.*s'\n", 
	    fp->orig, value.s.len, ZSW(value.s.s));
    } else {
	LOG(L_INFO, "AVP: '%s'=%d\n", fp->orig, value.n);
    }
    return 1;
}


static int del_attr(struct sip_msg* msg, char* p1, char* p2)
{
    fparam_t* fp;
    avp_t* avp;
    struct search_state st;	
    
    fp = (fparam_t*)p1;
    
    avp = search_avp(fp->v.avp, 0, &st);
    while (avp) {
	destroy_avp(avp);
	avp = search_next_avp(&st, 0);
    }
    return 1;
}


static int del_attrs(struct sip_msg* msg, char* p1, char* p2)
{
    return (reset_avp_list((unsigned long)p1) == 0) ? 1 : -1;
}
			

static int subst_attr_fixup(void** param, int param_no)
{
    if (param_no == 1) {
		return avpid_fixup(param, 1);
    }
    if (param_no == 2) {
		if (fix_param(FPARAM_SUBST, param) != 0) return -1;
    }
    return 0;
}


static int subst_attr(struct sip_msg* msg, char* p1, char* p2)
{
    avp_t* avp;
    avp_value_t val;
    str *res = NULL;
    int count;
    avp_ident_t* name = &((fparam_t*)p1)->v.avp;

    if ((avp = search_avp(*name, &val, NULL))) {
	if (avp->flags & AVP_VAL_STR) {
	    res = subst_str(val.s.s, msg, ((fparam_t*)p2)->v.subst, &count);
	    if (res == NULL) {
		ERR("avp_subst: error while running subst\n");
		goto error;
	    }

	    DBG("avp_subst: %d, result %.*s\n", count, res->len, ZSW(res->s));
	    val.s = *res;
	    
	    if (add_avp_before(avp, name->flags | AVP_VAL_STR, name->name, val)) {
		ERR("avp_subst: error while adding new AVP\n");
		goto error;
	    }
	    
	    destroy_avp(avp);
	    return 1;
	} else {
	    ERR("avp_subst: AVP has numeric value\n");
	    goto error;
	}
    } else {
	ERR("avp_subst: AVP[%.*s] index %d, flags %x not found\n", 
	    name->name.s.len, name->name.s.s,
	    name->index, name->flags);
	goto error;
    }

 error:
    if (res) pkg_free(res);
    return -1;
}


static int flags2attr(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t* id;
    int_str value;
    
    value.n = msg->flags;
    
    id = &((fparam_t*)p1)->v.avp;

    if (add_avp(id->flags, id->name, value) != 0) {
	ERR("add_avp failed\n");
	return -1;
    }
    
    return 1;
}


static int fixup_part(void** param, int param_no) 
{
    int i;
    fparam_t* fp;
    
    static struct {
		char* s;
		int i;
    } fixup_parse[] = {
		{"", SET_URI_T},
		{"prefix", PREFIX_T},
		{"uri", SET_URI_T},
		{"username", SET_USER_T},
		{"user", SET_USER_T},
		{"usernamepassword", SET_USERPASS_T},
		{"userpass", SET_USERPASS_T},
		{"domain", SET_HOST_T},
		{"host", SET_HOST_T},
		{"domainport", SET_HOSTPORT_T},
		{"hostport", SET_HOSTPORT_T},
		{"port", SET_PORT_T},
		{"strip", STRIP_T},
		{"strip_tail", STRIP_TAIL_T},
		{0, 0}
    };
    
    if (param_no == 1) {
		return avpid_fixup(param, 1);
    } else if (param_no == 2) {
		/* Create fparam structure */
		if (fix_param(FPARAM_STRING, param) != 0) return -1;
		
		/* We will parse the string now and store the value
		 * as int
		 */
		fp = (fparam_t*)*param;
		fp->type = FPARAM_INT;
		
		for(i = 0; fixup_parse[i].s; i++) {
			if (!strcasecmp(fp->orig, fixup_parse[i].s)) {
				fp->v.i = fixup_parse[i].i;
				return 1;
			}
		}
		
		ERR("Invalid parameter value: '%s'\n", fp->orig);
		return -1;
    }
    return 0;
}


static int attr2uri(struct sip_msg* msg, char* p1, char* p2)
{
    int_str value;
    avp_t* avp_entry;
    struct action act;
	struct run_act_ctx ra_ctx;
    int pnr;
    unsigned int u;
    
    if (p2) {
		pnr = ((fparam_t*)p2)->v.i;
    } else {
		pnr = SET_URI_T;
    }
    
    avp_entry = search_avp(((fparam_t*)p1)->v.avp, &value, NULL);
    if (avp_entry == 0) {
		ERR("attr2uri: AVP '%s' not found\n", ((fparam_t*)p1)->orig);
		return -1;
    }
    
    memset(&act, 0, sizeof(act));
	
    if ((pnr == STRIP_T) || (pnr == STRIP_TAIL_T)) {
		/* we need integer value for these actions */
        if (avp_entry->flags & AVP_VAL_STR) {
			if (str2int(&value.s, &u)) {
				ERR("not an integer value: %.*s\n",
					value.s.len, value.s.s);
				return -1;
			}
			act.val[0].u.number = u;
		} else {
			act.val[0].u.number = value.n;
		}
		act.val[0].type = NUMBER_ST;
    } else {
		/* we need string value */
		if ((avp_entry->flags & AVP_VAL_STR) == 0) {
			act.val[0].u.string = int2str(value.n, NULL);
		} else {
			act.val[0].u.string = value.s.s;
		}
		act.val[0].type = STRING_ST;
    }
    act.type = pnr;
    init_run_actions_ctx(&ra_ctx);
    if (do_action(&ra_ctx, &act, msg) < 0) {
		ERR("failed to change ruri part.\n");
		return -1;
    }
    return 1;
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
	switch(avp->flags&(AVP_NAME_STR|AVP_VAL_STR)) {
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


static int dump_attrs(struct sip_msg* m, char* x, char* y)
{
    avp_list_t avp_list;
    unsigned long flags;

    if (x) {
	flags = (unsigned long)x;
    } else {
	flags = AVP_CLASS_ALL | AVP_TRACK_ALL;
    }


    if (flags & AVP_CLASS_GLOBAL) {
	avp_list = get_avp_list(AVP_CLASS_GLOBAL);
	INFO("class=GLOBAL\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }

    if (flags & AVP_CLASS_DOMAIN && flags & AVP_TRACK_FROM) {
	avp_list = get_avp_list(AVP_CLASS_DOMAIN | AVP_TRACK_FROM);
	INFO("track=FROM class=DOMAIN\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }

    if (flags & AVP_CLASS_DOMAIN && flags & AVP_TRACK_TO) {
	avp_list = get_avp_list(AVP_CLASS_DOMAIN | AVP_TRACK_TO);
	INFO("track=TO class=DOMAIN\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }

    if (flags & AVP_CLASS_USER && flags & AVP_TRACK_FROM) {
	avp_list = get_avp_list(AVP_CLASS_USER | AVP_TRACK_FROM);
	INFO("track=FROM class=USER\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }

    if (flags & AVP_CLASS_USER && flags & AVP_TRACK_TO) {
	avp_list = get_avp_list(AVP_CLASS_USER | AVP_TRACK_TO);
	INFO("track=TO class=USER\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }

    if (flags & AVP_CLASS_URI && flags & AVP_TRACK_FROM) {
	avp_list = get_avp_list(AVP_CLASS_URI | AVP_TRACK_FROM);
	INFO("track=FROM class=URI\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }	

    if (flags & AVP_CLASS_URI && flags & AVP_TRACK_TO) {
	avp_list = get_avp_list(AVP_CLASS_URI | AVP_TRACK_TO);
	INFO("track=TO class=URI\n");
	if (!avp_list) {
	    LOG(L_INFO,"INFO: No AVP present\n");
	} else {
	    dump_avp_reverse(avp_list);
	}
    }
    return 1;
}


/*
 *  returns 1 if msg contains an AVP with the given name and value,
 *  returns -1 otherwise
 */
static int attr_equals(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t avpid;
    int_str value, avp_value;
    avp_t* avp;
    struct search_state st;

    if (get_avp_id(&avpid, (fparam_t*)p1, msg) < 0) {
	return -1;
    }

    if (p2 && get_str_fparam(&value.s, msg, (fparam_t*)p2) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p2)->orig);
	return -1;
    }

    avp = search_avp(avpid, &avp_value, &st);
    if (avp == 0) return -1;

    if (!p2) return 1;
    
    while (avp != 0) {
	if (avp->flags & AVP_VAL_STR) {
	    if ((avp_value.s.len == value.s.len) &&
		!memcmp(avp_value.s.s, value.s.s, avp_value.s.len)) {
		return 1;
	    }
	} else {
	    if (avp_value.n == str2s(value.s.s, value.s.len, 0)) {
		return 1;
	    }
	}
	avp = search_next_avp(&st, &avp_value);
    }
    
    return -1;
}


static int attr_exists(struct sip_msg* msg, char* p1, char* p2)
{
	return attr_equals(msg, p1, NULL);
}


static int xl_printstr(struct sip_msg* msg, xl_elog_t* format, char** res, int* res_len)
{
    int len;
    
    if (!format || !res) {
	LOG(L_ERR, "xl_printstr: Called with null format or res\n");
	return -1;
    }
    
    if (!xlbuf) {
	xlbuf = pkg_malloc((xlbuf_size+1)*sizeof(char));
	if (!xlbuf) {
	    LOG(L_CRIT, "xl_printstr: No memory left for format buffer\n");
	    return -1;
	}
    }
    
    len = xlbuf_size;
    if (xl_print(msg, format, xlbuf, &len)<0) {
	LOG(L_ERR, "xl_printstr: Error while formating result\n");
	return -1;
    }
    
    if ((xl_nul) && (xl_nul->len == len) && !strncmp(xl_nul->s, xlbuf, len)) {
	return 0;
    }

    *res = xlbuf;
    if (res_len) {
	*res_len=len;
    }
    return len;
}


static int attr_equals_xl(struct sip_msg* msg, char* p1, char* format)
{
    avp_ident_t* avpid;
    avp_value_t avp_val;
    struct search_state st;
    str xl_val;
    avp_t* avp;
    
    avpid = &((fparam_t*)p1)->v.avp;

    if (xl_printstr(msg, (xl_elog_t*) format, &xl_val.s, &xl_val.len) > 0) {
	for (avp = search_avp(*avpid, &avp_val, &st); avp; avp = search_next_avp(&st, &avp_val)) {
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

/* get the pointer to the xl lib functions */
static int get_xl_functions(void)
{
    if (!xl_print) {
	xl_print=(xl_print_log_f*)find_export("xprint", NO_SCRIPT, 0);
	
	if (!xl_print) {
	    LOG(L_CRIT,"ERROR: cannot find \"xprint\", is module xprint loaded?\n");
	    return -1;
	}
    }
    
    if (!xl_parse) {
	xl_parse=(xl_parse_format_f*)find_export("xparse", NO_SCRIPT, 0);
	
	if (!xl_parse) {
	    LOG(L_CRIT,"ERROR: cannot find \"xparse\", is module xprint loaded?\n");
	    return -1;
	}
    }

    if (!xl_free) {
	xl_free=(xl_elog_free_all_f*)find_export("xfree", NO_SCRIPT, 0);
	
	if (!xl_free) {
	    LOG(L_CRIT,"ERROR: cannot find \"xfree\", is module xprint loaded?\n");
	    return -1;
	}
    }

    if (!xl_nul) {
	xl_getnul=(xl_get_nulstr_f*)find_export("xnulstr", NO_SCRIPT, 0);
	if (xl_getnul) {
	    xl_nul=xl_getnul();
	}
	
	if (!xl_nul){
	    LOG(L_CRIT,"ERROR: cannot find \"xnulstr\", is module xprint loaded?\n");
	    return -1;
	} else {
	    LOG(L_INFO,"INFO: xprint null is \"%.*s\"\n", xl_nul->len, xl_nul->s);
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

    if (get_xl_functions()) return -1;

    if (param_no == 1) {
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
	return avpid_fixup(param, 1);
    } else  if (param_no == 2) {
	return fixup_xl_1(param, 1);
    }
    return 0;
}


static int xlset_attr(struct sip_msg* msg, char* p1, char* format)
{
    avp_ident_t* avpid;
    avp_value_t val;
    
    avpid = &((fparam_t*)p1)->v.avp;

    if (xl_printstr(msg, (xl_elog_t*)format, &val.s.s, &val.s.len) > 0) {
	if (add_avp(avpid->flags | AVP_VAL_STR, avpid->name, val)) {
	    ERR("xlset_attr:Error adding new AVP\n");
	    return -1;
	}
	return 1;
    }
    
    ERR("xlset_attr:Error while expanding xl_format\n");
    return -1;
}

/*
 * get the xl function pointers and fix up the AVP parameter
 */
static int xlfix_attr_fixup(void** param, int param_no)
{
    if (get_xl_functions()) return -1;

    if (param_no == 1)
	return avpid_fixup(param, 1);

    return 0;
}

/* fixes an attribute containing xl formatted string to pure string runtime */
static int xlfix_attr(struct sip_msg* msg, char* p1, char* p2)
{
    avp_t* avp;
    avp_ident_t* avpid;
    avp_value_t val;
    xl_elog_t* format=NULL;
    int ret=-1;
    
    avpid = &((fparam_t*)p1)->v.avp;

    /* search the AVP */
    avp = search_avp(*avpid, &val, 0);
    if (!avp) {
	DBG("xlfix_attr: AVP does not exist\n");
	goto error;
    }
    if ((avp->flags & AVP_VAL_STR) == 0) {
	DBG("xlfix_attr: Not a string AVP\n");
	goto error;
    }

    /* parse the xl syntax -- AVP values are always
    zero-terminated */
    if (xl_parse(val.s.s, &format)<0) {
	LOG(L_ERR, "ERROR: xlfix_attr: wrong format[%s]\n", val.s.s);
	goto error;
    }

    if (xl_printstr(msg, format, &val.s.s, &val.s.len) > 0) {
	/* we must delete and re-add the AVP again */
	destroy_avp(avp);
	if (add_avp(avpid->flags | AVP_VAL_STR, avpid->name, val)) {
	    ERR("xlfix_attr:Error adding new AVP\n");
	    goto error;
	}
	/* everything went OK */
	ret = 1;
    }

error:
    /* free the parsed xl expression */
    if (format) xl_free(format);

    return ret;
}


static int request_hf_helper(struct sip_msg* msg, str* hf, avp_ident_t* ident, struct lump* anchor, struct search_state* st, int front, int reverse, int reply)
{
    struct lump* new_anchor;
    static struct search_state state;
    avp_t* avp;
    char* s;
    str fin_val;
    int len, ret;
    int_str val;
    struct hdr_field* pos, *found = NULL;
    
    if (!anchor && !reply) {
	
	if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
	    LOG(L_ERR, "ERROR: request_hf_helper: Error while parsing message\n");
	    return -1;
	}
	
	pos = msg->headers;
	while (pos && (pos->type != HDR_EOH_T)) {
	    if ((hf->len == pos->name.len)
		&& (!strncasecmp(hf->s, pos->name.s, pos->name.len))) {
		found = pos;
		if (front) {
		    break;
		}
	    }
	    pos = pos->next;
	}
	
	if (found) {
	    if (front) {
		len = found->name.s - msg->buf;
	    } else {
		len = found->name.s + found->len - msg->buf;
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
	new_anchor = anchor;
    }
    
    if (!st) {
	st = &state;
	avp = search_avp(*ident, NULL, st);
	ret = -1;
    } else {
	avp = search_next_avp(st, NULL);
	ret = 1;
    }
    
    if (avp) {
	if (reverse && (request_hf_helper(msg, hf, ident, new_anchor, st, front, reverse, reply) == -1)) {
	    return -1;
	}
	
	get_avp_val(avp, &val);
	if (avp->flags & AVP_VAL_STR) {
	    fin_val = val.s;
	} else {
	    fin_val.s = int2str(val.n, &fin_val.len);
	}
	
	len = hf->len + 2 + fin_val.len + 2;
	s = (char*)pkg_malloc(len);
	if (!s) {
	    LOG(L_ERR, "ERROR: request_hf_helper: No memory left for data lump\n");
	    return -1;
	}
	
	memcpy(s, hf->s, hf->len);
	memcpy(s + hf->len, ": ", 2 );
	memcpy(s + hf->len+2, fin_val.s, fin_val.len );
	memcpy(s + hf->len + 2 + fin_val.len, CRLF, CRLF_LEN);
	
	if (reply) {
	    if (add_lump_rpl( msg, s, len, LUMP_RPL_HDR | LUMP_RPL_NODUP) == 0) {
		LOG(L_ERR, "ERROR: request_hf_helper: Can't insert RPL lump\n");
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
	if (!reverse && (request_hf_helper(msg, hf, ident, new_anchor, st, front, reverse, reply) == -1)) {
	    return -1;
	}
	return 1;
    };
    
	 /* in case of topmost call (st==NULL) return error */
	 /* otherwise it's OK, no more AVPs found */
    return ret; 
}


static int fixup_str_1_attr_2(void** param, int param_no)
{
    if (param_no == 1) {
	return fixup_var_str_12(param, 1);
    } else if (param_no == 2) {
	return avpid_fixup(param, 1);
    }
    return 0;
}


static int insert_req(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t ident, *avp;
    str hf;
    
    if (get_str_fparam(&hf, msg, (fparam_t*)p1) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }

    if (p2) {
	avp = &((fparam_t*)p2)->v.avp;
    } else {
	ident.name.s = hf;
	ident.flags = AVP_NAME_STR;
	ident.index = 0;
	avp = &ident;
    }
    return (request_hf_helper(msg, &hf, avp, NULL, NULL, 1, 0, 0));
}


static int append_req(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t ident, *avp;
    str hf;

    if (get_str_fparam(&hf, msg, (fparam_t*)p1) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
    if (p2) {
	avp = &((fparam_t*)p2)->v.avp;
    } else {
	ident.name.s = hf;
	ident.flags = AVP_NAME_STR;
	ident.index = 0;
	avp = &ident;
    }
    return (request_hf_helper(msg, &hf, avp, NULL, NULL, 0, 1, 0));
}


static int replace_req(struct sip_msg* msg, char* p1, char* p2)
{
    struct hdr_field* pos;
    str hf;
    
    if (get_str_fparam(&hf, msg, (fparam_t*)p1) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
    if (parse_headers(msg, HDR_EOH_F, 0) == -1) {
	LOG(L_ERR, "ERROR: replace_req: Error while parsing message\n");
	return -1;
    }
    
    pos = msg->headers;
    while (pos && (pos->type != HDR_EOH_T)) {
	if (hf.len == pos->name.len
	    && !strncasecmp(hf.s, pos->name.s, pos->name.len)) {
	    if (del_lump(msg, pos->name.s - msg->buf, pos->len, 0) == 0) {
		LOG(L_ERR,"ERROR: Can't insert del lump\n");
		return -1;
	    }
	}
	pos = pos->next;
    }
    return append_req(msg, p1, p2);
}


static int append_reply(struct sip_msg* msg, char* p1, char* p2)
{
    avp_ident_t ident, *avp;
    str hf;
    
    if (get_str_fparam(&hf, msg, (fparam_t*)p1) < 0) {
	ERR("Error while obtaining attribute value from '%s'\n", ((fparam_t*)p1)->orig);
	return -1;
    }
    
    if (p2) {
	avp = &((fparam_t*)p2)->v.avp;
    } else {
	ident.name.s = hf;
	ident.flags = AVP_NAME_STR;
	ident.index = 0;
	avp = &ident;
    }
    return (request_hf_helper(msg, &hf, avp, NULL, NULL, 0, 1, 1));
}


static int set_destination(struct sip_msg* msg, str* dest)
{
    name_addr_t nameaddr;
    
    if (!parse_nameaddr(dest, &nameaddr)) {
	return set_dst_uri(msg, &nameaddr.uri);
    } else {
	     /* it is just URI, pass it through */
	return set_dst_uri(msg, dest);
    }
}


static int attr_destination(struct sip_msg* msg, char* p1, char* p2)
{
    avp_t* avp;
    avp_value_t val;
    
    if ((avp = search_avp(((fparam_t*)p1)->v.avp, &val, NULL))) {
	if (avp->flags & AVP_VAL_STR) {
	    if (set_destination(msg, &val.s)) {
		LOG(L_ERR, "ERROR: avp_destination: Can't set dst uri\n");
		return -1;
	    };
		/* dst_uri changed, so it makes sense to re-use the current uri for
			forking */
		ruri_mark_new(); /* re-use uri for serial forking */
	    return 1;
	} else {
	    ERR("avp_destination:AVP has numeric value\n");
	    return -1;
	}
    }
    return -1;
}


static int xlset_destination(struct sip_msg* msg, char* format, char* p2)
{
    str val;
    
    if (xl_printstr(msg, (xl_elog_t*) format, &val.s, &val.len) > 0) {
	DBG("Setting dest to: '%.*s'\n", val.len, val.s);
	if (set_destination(msg, &val) == 0) {
	    return 1;
	}
    }
    
    return -1;
}


static int attr_hdr_body2attrs(struct sip_msg* m, char* header_, char* prefix_)
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
	    LOG(L_ERR, "ERROR: attr_hdr_body2attrs: Error while parsing message\n");
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
	    LOG(L_ERR, "ERROR: attr_hdr_body2attrs: Error while parsing message\n");
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
			    DBG("DEBUG: attr_hdr_body2attrs: adding avp '%.*s', sval: '%.*s'\n", name2.s.len, (char*) name2.s.s, val.len, val.s);
			} else {
			    DBG("DEBUG: attr_hdr_body2attrs: adding avp '%.*s', ival: '%d'\n", name2.s.len, (char*) name2.s.s, val2.n);
			}
			if ( add_avp(AVP_NAME_STR | val_type, name2, val2)!=0) {
			    LOG(L_ERR, "ERROR: attr_hdr_body2attrs: add_avp failed\n");
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


static int attr_hdr_body2attrs2(struct sip_msg* msg, char* header_, char* prefix_) 
{
    return attr_hdr_body2attrs(msg, header_, prefix_);
}


static int attr_hdr_body2attrs_fixup(void** param, int param_no) {
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
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: bad AVP value\n");
		return E_CFG;
	    }
	    switch (n) {
		//				case HDR_xxx:
		//				case HDR_xxx:
		//					break;
	    default:
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: header name is not valid and supported HDR_xxx id '%s' resolved as %d\n", c, n);
		return E_CFG;
	    }
	    h = pkg_malloc(sizeof(*h));
	    if (!h) {
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: out of memory\n");
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
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: header name is empty\n");
		return E_CFG;
	    }
	    h = pkg_malloc(sizeof(*h)+n+1);
	    if (!h) {
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: out of memory\n");
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
		    LOG(L_ERR, "attr_hdr_body2attrs_fixup: bad field param modifier near '%s'\n", params);
		    return E_CFG;
		}
		params++;
	    }
	    if (!h->val_types) {
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: no field param modifier specified\n");
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
		LOG(L_ERR, "attr_hdr_body2attrs_fixup: out of memory\n");
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

static int attr_hdr_body2attrs2_fixup(void** param, int param_no) 
{
    struct hdr_name *h;
    int res = attr_hdr_body2attrs_fixup(param, param_no);
    if (res == 0 && param_no == 1) {
	h = *param;
	h->field_delimiter = ';';
	h->array_delimiter = ',';
    }
    return res;
}



static int avpgroup_fixup(void** param, int param_no)
{
    unsigned long flags;
    char* s;
    
    if (param_no == 1) {
	     /* Determine the track and class of attributes to be loaded */
	s = (char*)*param;
	flags = 0;
	if (*s != '$' || (strlen(s) != 3 && strlen(s) != 2)) {
	    ERR("Invalid parameter value, $xy expected\n");
	    return -1;
	}
	switch((s[1] << 8) + s[2]) {
	case 0x4655: /* $fu */
	case 0x6675:
	case 0x4675:
	case 0x6655:
	    flags = AVP_TRACK_FROM | AVP_CLASS_USER;
	    break;
	    
	case 0x4652: /* $fr */
	case 0x6672:
	case 0x4672:
	case 0x6652:
	    flags = AVP_TRACK_FROM | AVP_CLASS_URI;
	    break;
	    
	case 0x5455: /* $tu */
	case 0x7475:
	case 0x5475:
	case 0x7455:
	    flags = AVP_TRACK_TO | AVP_CLASS_USER;
	    break;
	    
	case 0x5452: /* $tr */
	case 0x7472:
	case 0x5472:
	case 0x7452:
	    flags = AVP_TRACK_TO | AVP_CLASS_URI;
	    break;

	case 0x4644: /* $fd */
	case 0x6664:
	case 0x4664:
	case 0x6644:
	    flags = AVP_TRACK_FROM | AVP_CLASS_DOMAIN;
	    break;

	case 0x5444: /* $td */
	case 0x7464:
	case 0x5464:
	case 0x7444:
	    flags = AVP_TRACK_TO | AVP_CLASS_DOMAIN;
	    break;

	case 0x6700: /* $td */
	case 0x4700:
	    flags = AVP_CLASS_GLOBAL;
	    break;
	    
	default:
	    ERR("Invalid parameter value: '%s'\n", s);
	    return -1;
	}
	
	pkg_free(*param);
	*param = (void*)flags;
	return 1;
    }
    return 0;
}



static int select_attr_fixup(str* res, select_t* s, struct sip_msg* msg)
{
	avp_ident_t *avp_ident;

#define SEL_PARAM_IDX	1

	if (! msg) { /* fixup call */
		str attr_name;
		
		if (s->params[SEL_PARAM_IDX].type != SEL_PARAM_STR) {
			ERR("attribute name expected.\n");
			return -1;
		}

		attr_name = s->params[SEL_PARAM_IDX].v.s;
		DEBUG("fix up for attribute '%.*s'\n", STR_FMT(&attr_name));

		if (! (avp_ident = pkg_malloc(sizeof(avp_ident_t)))) {
			ERR("out of mem; requested: %d.\n", (int)sizeof(avp_ident_t));
			return -1;
		}
		memset(avp_ident, 0, sizeof(avp_ident_t));

		/* skip leading `$' */
		if ((1 < attr_name.len) && (attr_name.s[0] == '$')) {
			attr_name.len --;
			attr_name.s ++;
		}
		if (parse_avp_ident(&attr_name, avp_ident) < 0) {
			ERR("failed to parse attribute name: `%.*s'.\n", STR_FMT(&attr_name));
			pkg_free(avp_ident);
		}
		s->params[SEL_PARAM_IDX].v.p = avp_ident;
		s->params[SEL_PARAM_IDX].type = SEL_PARAM_PTR;
	} else { /* run time call */
		avp_t *ret;
		avp_value_t val;

#ifdef EXTRA_DEBUG
		assert(s->params[SEL_PARAM_IDX].type == SEL_PARAM_PTR);
#endif
		avp_ident = s->params[SEL_PARAM_IDX].v.p;
		ret = search_first_avp(avp_ident->flags, avp_ident->name, &val, NULL);
		if (ret && ret->flags & AVP_VAL_STR)
			*res = val.s;
	}

	return 0;

#undef SEL_PARAM_IDX
}

SELECT_F(select_any_nameaddr)
ABSTRACT_F(select_attr);

select_row_t sel_declaration[] = {
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("avp"), select_attr, SEL_PARAM_EXPECTED},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("attr"), select_attr, SEL_PARAM_EXPECTED},
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("attribute"), select_attr, SEL_PARAM_EXPECTED},
	{ select_attr, SEL_PARAM_STR, STR_NULL, select_attr_fixup, FIXUP_CALL | CONSUME_NEXT_STR},

	{ select_attr_fixup, SEL_PARAM_STR, STR_STATIC_INIT("nameaddr"), select_any_nameaddr, NESTED},

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};

static int mod_init()
{
	DBG("%s - initializing\n", exports.name);
	return register_select_table(sel_declaration);
}
