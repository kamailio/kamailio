/*
 * $Id$
 *
 * PERMISSIONS module
 *
 * Copyright (C) 2003 Miklós Tirpák (mtirpak@sztaki.hu)
 * Copyright (C) 2003 iptel.org
 * Copyright (C) 2003 Juha Heinanen (jh@tutpro.com)
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
 *
 */
 
#include <stdio.h>
#include "permissions.h"
#include "parse_config.h"
#include "trusted.h"
#include "../../mem/mem.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/contact/parse_contact.h"
#include "../../str.h"
#include "../../dset.h"
#include "../../globals.h"

MODULE_VERSION

static rule_file_t allow[MAX_RULE_FILES]; /* Parsed allow files */
static rule_file_t deny[MAX_RULE_FILES];  /* Parsed deny files */
static int rules_num;  /* Number of parsed allow/deny files */


/* Module parameter variables */
static char* default_allow_file = DEFAULT_ALLOW_FILE;
static char* default_deny_file = DEFAULT_DENY_FILE;
static char* allow_suffix = ".allow";
static char* deny_suffix = ".deny";


/* for allow_trusted function */
char* db_url = 0;                  /* Don't connect to the database by default */
int db_mode = DISABLE_CACHE;	   /* Database usage mode: 0=no cache, 1=cache */
char* trusted_table = "trusted";   /* Name of trusted table */
char* source_col = "src_ip";       /* Name of source address column */
char* proto_col = "proto";         /* Name of protocol column */
char* from_col = "from_pattern";   /* Name of from pattern column */

db_con_t* db_handle = 0;

/*
 * By default we check all branches
 */
static int check_all_branches = 1;


/*  
 * Convert the name of the files into table index
 */
static int load_fixup(void** param, int param_no);

/*
 * Convert the name of the file into table index, this
 * function takes just one name, appends .allow and .deny
 * to and and the rest is same as in load_fixup
 */
static int single_fixup(void** param, int param_no);

static int allow_routing_0(struct sip_msg* msg, char* str1, char* str2);
static int allow_routing_1(struct sip_msg* msg, char* basename, char* str2);
static int allow_routing_2(struct sip_msg* msg, char* allow_file, char* deny_file);
static int allow_register_1(struct sip_msg* msg, char* basename, char* s);
static int allow_register_2(struct sip_msg* msg, char* allow_file, char* deny_file);

static int mod_init(void);
static void mod_exit(void);
static int child_init(int rank);


/* Exported functions */
static cmd_export_t cmds[] = {
        {"allow_routing",  allow_routing_0,  0, 0,             REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_routing",  allow_routing_1,  1, single_fixup,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_routing",  allow_routing_2,  2, load_fixup,    REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_register", allow_register_1, 1, single_fixup,  REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_register", allow_register_2, 2, load_fixup,    REQUEST_ROUTE | FAILURE_ROUTE},
	{"allow_trusted",  allow_trusted,    0, 0,             REQUEST_ROUTE | FAILURE_ROUTE},
        {0, 0, 0, 0, 0}
};

/* Exported parameters */
static param_export_t params[] = {
        {"default_allow_file", STR_PARAM, &default_allow_file},
        {"default_deny_file",  STR_PARAM, &default_deny_file },
	{"check_all_branches", INT_PARAM, &check_all_branches},
	{"allow_suffix",       STR_PARAM, &allow_suffix      },
	{"deny_suffix",        STR_PARAM, &deny_suffix       },
	{"db_url",             STR_PARAM, &db_url            },
	{"db_mode",            INT_PARAM, &db_mode           },
	{"trusted_table",      STR_PARAM, &trusted_table     },
	{"source_col",         STR_PARAM, &source_col        },
	{"proto_col",          STR_PARAM, &proto_col         },
	{"from_col",           STR_PARAM, &from_col          },
        {0, 0, 0}
};

/* Module interface */
struct module_exports exports = {
        "permissions",
        cmds,      /* Exported functions */
        params,    /* Exported parameters */
        mod_init,  /* module initialization function */
        0,         /* response function */
        mod_exit,  /* destroy function */
        0,         /* oncancel function */
        child_init /* child initialization function */
};


/*
 * Extract path (the begining of the string
 * up to the last / character
 * Returns length of the path
 */
static int get_path(char* pathname)
{
	char* c;
	if (!pathname) return 0;
	
	c = strrchr(pathname, '/');
	if (!c) return 0;

	return c - pathname + 1;
}


/*
 * Prepend path if necesarry
 */
static char* get_pathname(char* name)
{
	char* buffer;
	int path_len, name_len;

	if (!name) return 0;
	
	name_len = strlen(name);
	if (strchr(name, '/')) {
		buffer = (char*)pkg_malloc(name_len + 1);
		if (!buffer) goto err;
		strcpy(buffer, name);
		return buffer;
	} else {
		path_len = get_path(cfg_file);
		buffer = (char*)pkg_malloc(path_len + name_len + 1);
		if (!buffer) goto err;
		memcpy(buffer, cfg_file, path_len);
		memcpy(buffer + path_len, name, name_len);
		buffer[path_len + name_len] = '\0';
		return buffer;
	}

 err:
	LOG(L_ERR, "get_pathname(): No memory left\n");
	return 0;
}


/*
 * If the file pathname has been parsed already then the
 * function returns its index in the tables, otherwise it
 * returns -1 to indicate that the file needs to be read
 * and parsed yet
 */
static int find_index(rule_file_t* array, char* pathname)
{
	int i;

	for(i = 0; i < rules_num; i++) {
		if (!strcmp(pathname, array[i].filename)) return i;
	}

	return -1;
}


/*
 * Return URI without all the bells and whistles, that means only
 * sip:username@domain, resulting buffer is statically allocated and
 * zero terminated
 */
static char* get_plain_uri(const str* uri)
{
	static char buffer[EXPRESSION_LENGTH + 1];
	struct sip_uri puri;
	int len;

	if (!uri) return 0;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "get_plain_uri(): Error while parsing URI\n");
		return 0;
	}
	
	if (puri.user.len) {
		len = puri.user.len + puri.host.len + 5;
	} else {
		len = puri.host.len + 4;
	}

	if (len > EXPRESSION_LENGTH) {
		LOG(L_ERR, "allow_register(): (module permissions) Request-URI is too long: %d chars\n", len);
		return 0;
	}
	
	strcpy(buffer, "sip:");
	if (puri.user.len) {
		memcpy(buffer + 4, puri.user.s, puri.user.len);
	        buffer[puri.user.len + 4] = '@';
		memcpy(buffer + puri.user.len + 5, puri.host.s, puri.host.len);
	} else {
		memcpy(buffer + 4, puri.host.s, puri.host.len);
	}

	buffer[len] = '\0';
	return buffer;
}


/*
 * determinates the permission of the call
 * return values:
 * -1:	deny
 * 1:	allow
 */
static int check_routing(struct sip_msg* msg, int idx) 
{
	struct hdr_field *from;
	int len;
	static char from_str[EXPRESSION_LENGTH+1];
	static char ruri_str[EXPRESSION_LENGTH+1];
	char* uri_str;
	str branch;
	
	/* turn off control, allow any routing */
	if ((!allow[idx].rules) && (!deny[idx].rules)) {
		DBG("check_routing(): No rules => allow any routing\n");
		return 1;
	}
	
	/* looking for FROM HF */
        if ((!msg->from) && (parse_headers(msg, HDR_FROM, 0) == -1)) {
                LOG(L_ERR, "check_routing(): Error while parsing message\n");
                return -1;
        }
	
	if (!msg->from) {
		LOG(L_ERR, "check_routing(): FROM header field not found\n");
		return -1;
	}
	
	/* we must call parse_from_header explicitly */
        if ((!(msg->from)->parsed) && (parse_from_header(msg) < 0)) {
                LOG(L_ERR, "check_routing(): Error while parsing From body\n");
                return -1;
        }
	
	from = msg->from;
	len = ((struct to_body*)from->parsed)->uri.len;
	if (len > EXPRESSION_LENGTH) {
                LOG(L_ERR, "check_routing(): From header field is too long: %d chars\n", len);
                return -1;
	}
	strncpy(from_str, ((struct to_body*)from->parsed)->uri.s, len);
	from_str[len] = '\0';
	
	/* looking for request URI */
	if (parse_sip_msg_uri(msg) < 0) {
	        LOG(L_ERR, "check_routing(): uri parsing failed\n");
	        return -1;
	}
	
	len = msg->parsed_uri.user.len + msg->parsed_uri.host.len + 5;
	if (len > EXPRESSION_LENGTH) {
                LOG(L_ERR, "check_routing(): Request URI is too long: %d chars\n", len);
                return -1;
	}
	
	strcpy(ruri_str, "sip:");
	memcpy(ruri_str + 4, msg->parsed_uri.user.s, msg->parsed_uri.user.len);
	ruri_str[msg->parsed_uri.user.len + 4] = '@';
	memcpy(ruri_str + msg->parsed_uri.user.len + 5, msg->parsed_uri.host.s, msg->parsed_uri.host.len);
	ruri_str[len] = '\0';
	
        DBG("check_routing(): looking for From: %s Request-URI: %s\n", from_str, ruri_str);
	     /* rule exists in allow file */
	if (search_rule(allow[idx].rules, from_str, ruri_str)) {
		if (check_all_branches) goto check_branches;
    		DBG("check_routing(): allow roule found => routing is allowed\n");
		return 1;
	}
	
	/* rule exists in deny file */
	if (search_rule(deny[idx].rules, from_str, ruri_str)) {
		DBG("check_routing(): deny roule found => routing is denied\n");
		return -1;
	}

	if (!check_all_branches) {
		DBG("check_routing(): Neither allow nor deny rule found => routing is allowed\n");
		return 1;
	}

 check_branches:
	init_branch_iterator();
	while((branch.s = next_branch(&branch.len))) {
		uri_str = get_plain_uri(&branch);
		if (!uri_str) {
			LOG(L_ERR, "check_uri(): Error while extracting plain URI\n");
			return -1;
		}
		DBG("check_routing: Looking for From: %s Branch: %s\n", from_str, uri_str);
		
		if (search_rule(allow[idx].rules, from_str, uri_str)) {
			continue;
		}
		
		if (search_rule(deny[idx].rules, from_str, uri_str)) {
			LOG(LOG_INFO, "check_routing(): Deny rule found for one of branches => routing is denied\n");
			return -1;
		}
	}
	
	LOG(LOG_INFO, "check_routing(): Check of branches passed => routing is allowed\n");
	return 1;
}


/*  
 * Convert the name of the files into table index
 */
static int load_fixup(void** param, int param_no)
{
	char* pathname;
	int idx;
	rule_file_t* table;

	if (param_no == 1) {
		table = allow;
	} else {
		table = deny;
	}

	pathname = get_pathname(*param);
	idx = find_index(table, pathname);

	if (idx == -1) {
		     /* Not opened yet, opent the file and parse it */
		table[rules_num].filename = pathname;
		table[rules_num].rules = parse_config_file(pathname);
		if (table[rules_num].rules) {
			LOG(L_INFO, "load_fixup(): File (%s) parsed\n", pathname);
		} else {
			LOG(L_WARN, "load_fixup(): File (%s) not found => empty rule set\n", pathname);
		}
		*param = (void*)rules_num;
		if (param_no == 2) rules_num++;
	} else {
		     /* File already parsed, re-use it */
		LOG(L_INFO, "load_fixup(): File (%s) already loaded, re-using\n", pathname);
		pkg_free(pathname);
		*param = (void*)idx;
	}

	return 0;
}


/*
 * Convert the name of the file into table index
 */
static int single_fixup(void** param, int param_no)
{
	char* buffer;
	void* tmp;
	int param_len, ret, suffix_len;

	if (param_no != 1) return 0;

	param_len = strlen((char*)*param);
	if (strlen(allow_suffix) > strlen(deny_suffix)) {
		suffix_len = strlen(allow_suffix);
	} else {
		suffix_len = strlen(deny_suffix);
	}

	buffer = pkg_malloc(param_len + suffix_len + 1);
	if (!buffer) {
		LOG(L_ERR, "single_fixup(): No memory left\n");
		return -1;
	}

	strcpy(buffer, (char*)*param);
	strcat(buffer, allow_suffix);
	tmp = buffer; 
	ret = load_fixup(&tmp, 1);

	strcpy(buffer + param_len, deny_suffix);
	tmp = buffer;
	ret |= load_fixup(&tmp, 2);

	*param = tmp;

	pkg_free(buffer);
	return ret;
}


/*
 * Contact header field iterator, returns next contact if any, it doesn't
 * parse message header if not absolutely necesarry
 */
static contact_t* contact_iterator(struct sip_msg* msg, contact_t* prev)
{
	static struct hdr_field* hdr = 0;
	struct hdr_field* last;
	contact_body_t* cb;

	if (!msg) return 0;

	if (!prev) {
		     /* No pointer to previous contact given, find topmost
		      * contact and return pointer to the first contact
		      * inside that header field
		      */
		hdr = msg->contact;
		cb = (contact_body_t*)hdr->parsed;
		return cb->contacts;
	} else {
		     /* Check if there is another contact in the
		      * same header field and if so then return it
		      */
		if (prev->next) return prev->next;

		     /* Try to find and parse another Contact
		      * header field
		      */
		last = hdr;
		hdr = hdr->next;

		     /* Search another already parsed Contact
		      * header field
		      */
		while(hdr && hdr->type != HDR_CONTACT) {
			hdr = hdr->next;
		}

		if (!hdr) {
			     /* Look for another Contact HF in unparsed
			      * part of the message header
			      */
			if (parse_headers(msg, HDR_CONTACT, 1) == -1) {
				LOG(L_ERR, "contact_iterator(): Error while parsing message header\n");
				return 0;
			}
			
			     /* Check if last found header field is Contact
			      * and if it is not the same header field as the
			      * previous Contact HF (that indicates that the previous 
			      * one was the last header field in the header)
			      */
			if ((msg->last_header->type == HDR_CONTACT) &&
			    (msg->last_header != last)) {
				hdr = msg->last_header;
			} else {
				return 0;
			}
		}
		
		if (parse_contact(hdr) < 0) {
			LOG(L_ERR, "contact_iterator(): Error while parsing Contact HF body\n");
			return 0;
		}
		
		     /* And return first contact within that
		      * header field
		      */
		cb = (contact_body_t*)hdr->parsed;
		return cb->contacts;
	}
}


/* 
 * module initialization function 
 */
static int mod_init(void)
{
	LOG(L_INFO, "permissions - initializing\n");

	allow[0].filename = get_pathname(DEFAULT_ALLOW_FILE);
        allow[0].rules = parse_config_file(allow[0].filename);
        if (allow[0].rules) {
                LOG(L_INFO, "Default allow file (%s) parsed\n", allow[0].filename);
	} else {
		LOG(L_WARN, "Default allow file (%s) not found => empty rule set\n", allow[0].filename);
	}
	
	deny[0].filename = get_pathname(DEFAULT_DENY_FILE);
	deny[0].rules = parse_config_file(deny[0].filename);
	if (deny[0].rules) {
		LOG(L_INFO, "Default deny file (%s) parsed\n", deny[0].filename);
	} else {
		LOG(L_WARN, "Default deny file (%s) not found => empty rule set\n", deny[0].filename);
	}

	if (init_trusted() != 0) {
		LOG(L_ERR, "Error while initializing allow_trusted function\n");
	}
	
	rules_num = 1;
	return 0;
}


static int child_init(int rank)
{
	return init_child_trusted(rank);
}


/* 
 * destroy function 
 */
static void mod_exit(void) 
{
	int i;

	for(i = 0; i < rules_num; i++) {
		free_rule(allow[i].rules);
		pkg_free(allow[i].filename);

		free_rule(deny[i].rules);
		pkg_free(deny[i].filename);
	}

	clean_trusted();
}


/*
 * Uses default rule files from the module parameters
 */
int allow_routing_0(struct sip_msg* msg, char* str1, char* str2)
{
	return check_routing(msg, 0);
}


int allow_routing_1(struct sip_msg* msg, char* basename, char* s)
{
	return check_routing(msg, (int)basename);
}


/*
 * Accepts allow and deny files as parameters
 */
int allow_routing_2(struct sip_msg* msg, char* allow_file, char* deny_file)
{
	     /* Index converted by load_lookup */
	return check_routing(msg, (int)allow_file);
}


/*
 * Test of REGISTER messages. Creates To-Contact pairs and compares them
 * against rules in allow and deny files passed as parameters. The function
 * iterates over all Contacts and creates a pair with To for each contact 
 * found. That allows to restrict what IPs may be used in registrations, for
 * example
 */
static int check_register(struct sip_msg* msg, int idx)
{
	int len;
	static char to_str[EXPRESSION_LENGTH + 1];
	char* contact_str;
	contact_t* c;

	     /* turn off control, allow any routing */
	if ((!allow[idx].rules) && (!deny[idx].rules)) {
		DBG("check_register(): No rules => allow any registration\n");
		return 1;
	}

	     /*
	      * Note: We do not parse the whole header field here although the message can
	      * contain multiple Contact header fields. We try contacts one by one and if one
	      * of them causes reject then we don't look at others, this could improve performance
	      * a little bit in some situations
	      */
	if (parse_headers(msg, HDR_TO | HDR_CONTACT, 0) == -1) {
		LOG(L_ERR, "check_register(): Error while parsing headers\n");
		return -1;
	}

	if (!msg->to || !msg->contact) {
		LOG(L_ERR, "check_register(): To or Contact not found\n");
		return -1;
	}
	
	     /* Check if the REGISTER message contains start Contact and if
	      * so then allow it
	      */
	if (parse_contact(msg->contact) < 0) {
		LOG(L_ERR, "check_register(): Error while parsing Contact HF\n");
		return -1;
	}

	if (((contact_body_t*)msg->contact->parsed)->star) {
		DBG("check_register(): * Contact found, allowing\n");
		return 1;
	}

	len = ((struct to_body*)msg->to->parsed)->uri.len;
	if (len > EXPRESSION_LENGTH) {
                LOG(L_ERR, "check_register(): To header field is too long: %d chars\n", len);
                return -1;
	}
	strncpy(to_str, ((struct to_body*)msg->to->parsed)->uri.s, len);
	to_str[len] = '\0';

	c = contact_iterator(msg, 0);

	while(c) {
		contact_str = get_plain_uri(&c->uri);		
		if (!contact_str) {
			LOG(L_ERR, "check_register(): Can't extract plain Contact URI\n");
			return -1;
		}

		DBG("check_register(): Looking for To: %s Contact: %s\n", to_str, contact_str);

		     /* rule exists in allow file */
		if (search_rule(allow[idx].rules, to_str, contact_str)) {
			if (check_all_branches) goto skip_deny;
		}
	
		     /* rule exists in deny file */
		if (search_rule(deny[idx].rules, to_str, contact_str)) {
			DBG("check_register(): Deny roule found => Register denied\n");
			return -1;
		}

	skip_deny:
		c = contact_iterator(msg, c);
	}

	DBG("check_register(): No contact denied => Allowed\n");
	return 1;
}


int allow_register_1(struct sip_msg* msg, char* basename, char* s)
{
	return check_register(msg, (int)basename);
}


int allow_register_2(struct sip_msg* msg, char* allow_file, char* deny_file)
{
	return check_register(msg, (int)allow_file);
}
