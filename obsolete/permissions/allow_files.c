/*
 * $Id$
 *
 * PERMISSIONS module
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 *
 * History:
 * --------
 *   2006-08-10: file operation functions are moved here (Miklos)
 */

#include "../../mem/mem.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_refer_to.h"
#include "../../parser/contact/parse_contact.h"
#include "../../str.h"
#include "../../dset.h"
#include "../../globals.h"
#include "rule.h"
#include "permissions.h"
#include "parse_config.h"
#include "allow_files.h"

/*
 * Extract path (the beginning of the string
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
 * Prepend path if necessary
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
static int find_index(rule_file_t* array, int num, char* pathname)
{
        int i;

        for(i = 0; i <= num; i++) {
                if (array[i].filename && !strcmp(pathname, array[i].filename)) return i;
        }

        return -1;
}

/* check if the file container array has been already created */
static int check_file_container(rule_file_t **_table)
{
        if (!*_table) {
                *_table = (rule_file_t *)pkg_malloc(sizeof(rule_file_t) * max_rule_files);
                if (!*_table) {
                        LOG(L_ERR, "ERROR: check_file_container(): not enough memory\n");
                        return -1;
                }
                memset(*_table, 0, sizeof(rule_file_t) * max_rule_files);
        }
        return 0;
}

/*
 * loads a config file into the array
 * return value:
 *   <0:  error
 *   >=0: index of the file
 *
 * _def must be true in case of the default file which
 * sets the index to 0
 */
int load_file(char *_name, rule_file_t **_table, int *_rules_num, int _def)
{
        char	*pathname;
        int	idx;
        rule_file_t	*table;
        int	err;

        if (check_file_container(_table)) return -1;

        table = *_table;
        pathname = get_pathname(_name);
        if (!pathname) return -1;
        if (_def) {
                /* default file, we must use the 0 index */
                idx = 0;
        } else {
                idx = find_index(table, *_rules_num, pathname);
        }

        if (idx == -1) {
                     /* Not opened yet, open the file and parse it */
                idx = *_rules_num + 1; /* skip the 0 index */
                if (idx >= max_rule_files) {
                        LOG(L_ERR, "ERROR: load_files(): array is too small to open the file,"\
                                        " increase max_rule_files module parameter!\n");
                        pkg_free(pathname);
                        return -1;
                }
                table[idx].filename = pathname;
                table[idx].rules = parse_config_file(pathname, &err);
                if (err) return -1;

                if (table[idx].rules) {
                        LOG(L_INFO, "load_files(): File (%s) parsed\n", pathname);
                } else {
                        LOG(L_WARN, "load_files(): File (%s) not found or empty => empty rule set\n", pathname);
                }
                LOG(L_DBG, "load_files(): filename:%s => idx:%d\n", pathname, idx);
                (*_rules_num)++;
                return idx;

        } else if (idx == 0) {
                /* default file, use index 0 and do not increase _rules_num */
                if (table[0].rules) {
                        LOG(L_INFO, "load_files(): File (%s) already loaded, re-using\n", pathname);
                        LOG(L_DBG, "load_files(): filename:%s => idx:%d\n", pathname, idx);
                        pkg_free(pathname);
                        return 0;
                }

                table[0].filename = pathname;
                table[0].rules = parse_config_file(pathname, &err);
                if (err) return -1;

                if (table[0].rules) {
                        LOG(L_INFO, "load_files(): File (%s) parsed\n", pathname);
                } else {
                        LOG(L_WARN, "load_files(): File (%s) not found or empty => empty rule set\n", pathname);
                }
                LOG(L_DBG, "load_files(): filename:%s => idx:%d\n", pathname, idx);
                return 0;

        } else {
                     /* File already parsed, re-use it */
                LOG(L_INFO, "load_files(): File (%s) already loaded, re-using\n", pathname);
                LOG(L_DBG, "load_files(): filename:%s => idx:%d\n", pathname, idx);
                pkg_free(pathname);
                return idx;
        }

}

/* free memory allocated for the file container */
void delete_files(rule_file_t **_table, int _num)
{
        int	i;
        rule_file_t	*table;

        if (!*_table) return;

        table = *_table;
        for(i = 0; i <= _num; i++) {
                if (table[i].rules) free_rule(table[i].rules);
                if (table[i].filename) pkg_free(table[i].filename);
        }
        pkg_free(*_table);
        *_table = NULL;
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
 * determines the permission of the call
 * return values:
 * -1:	deny
 * 1:	allow
 */
int check_routing(struct sip_msg* msg, int idx)
{
        struct hdr_field *from;
        int len, q;
        static char from_str[EXPRESSION_LENGTH+1];
        static char ruri_str[EXPRESSION_LENGTH+1];
        char* uri_str;
        str branch;

        /* turn off control, allow any routing */
        if (!allow || !deny || ((!allow[idx].rules) && (!deny[idx].rules))) {
                DBG("check_routing(): No rules => allow any routing\n");
                return 1;
        }

        /* looking for FROM HF */
        if ((!msg->from) && (parse_headers(msg, HDR_FROM_F, 0) == -1)) {
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
                DBG("check_routing(): allow rule found => routing is allowed\n");
                return 1;
        }

        /* rule exists in deny file */
        if (search_rule(deny[idx].rules, from_str, ruri_str)) {
                DBG("check_routing(): deny rule found => routing is denied\n");
                return -1;
        }

        if (!check_all_branches) {
                DBG("check_routing(): Neither allow nor deny rule found => routing is allowed\n");
                return 1;
        }

 check_branches:
        init_branch_iterator();
        while((branch.s = next_branch(&branch.len, &q, 0, 0, 0, 0))) {
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
                        LOG(L_INFO, "check_routing(): Deny rule found for one of branches => routing is denied\n");
                        return -1;
                }
        }

        LOG(L_INFO, "check_routing(): Check of branches passed => routing is allowed\n");
        return 1;
}

/*
 * Test of REGISTER messages. Creates To-Contact pairs and compares them
 * against rules in allow and deny files passed as parameters. The function
 * iterates over all Contacts and creates a pair with To for each contact
 * found. That allows to restrict what IPs may be used in registrations, for
 * example
 */
int check_register(struct sip_msg* msg, int idx)
{
        int len;
        static char to_str[EXPRESSION_LENGTH + 1];
        char* contact_str;
        contact_t* c;

             /* turn off control, allow any routing */
        if (!allow || !deny || ((!allow[idx].rules) && (!deny[idx].rules))) {
                DBG("check_register(): No rules => allow any registration\n");
                return 1;
        }

             /*
              * Note: We do not parse the whole header field here although the message can
              * contain multiple Contact header fields. We try contacts one by one and if one
              * of them causes reject then we don't look at others, this could improve performance
              * a little bit in some situations
              */
        if (parse_headers(msg, HDR_TO_F | HDR_CONTACT_F, 0) == -1) {
                LOG(L_ERR, "check_register(): Error while parsing headers\n");
                return -1;
        }

        if (!msg->to) {
                LOG(L_ERR, "check_register(): To or Contact not found\n");
                return -1;
        }

        if (!msg->contact) {
                     /* REGISTER messages that contain no Contact header field
                      * are allowed. Such messages do not modify the contents of
                      * the user location database anyway and thus are not harmful
                      */
                DBG("check_register(): No Contact found, allowing\n");
                return 1;
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

        if (contact_iterator(&c, msg, 0) < 0) {
                return -1;
        }

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
                        DBG("check_register(): Deny rule found => Register denied\n");
                        return -1;
                }

        skip_deny:
                if (contact_iterator(&c, msg, c) < 0) {
                        return -1;
                }
        }

        DBG("check_register(): No contact denied => Allowed\n");
        return 1;
}

/*
 * determines the permission to refer to given refer-to uri
 * return values:
 * -1:	deny
 * 1:	allow
 */
int check_refer_to(struct sip_msg* msg, int idx)
{
        struct hdr_field *from, *refer_to;
        int len;
        static char from_str[EXPRESSION_LENGTH+1];
        static char refer_to_str[EXPRESSION_LENGTH+1];

        /* turn off control, allow any refer */
        if (!allow || !deny || ((!allow[idx].rules) && (!deny[idx].rules))) {
                DBG("check_refer_to(): No rules => allow any refer\n");
                return 1;
        }

        /* looking for FROM HF */
        if ((!msg->from) && (parse_headers(msg, HDR_FROM_F, 0) == -1)) {
                LOG(L_ERR, "check_refer_to(): Error while parsing message\n");
                return -1;
        }

        if (!msg->from) {
                LOG(L_ERR, "check_refer_to(): FROM header field not found\n");
                return -1;
        }

        /* we must call parse_from_header explicitly */
        if ((!(msg->from)->parsed) && (parse_from_header(msg) < 0)) {
                LOG(L_ERR, "check_refer_to(): Error while parsing From body\n");
                return -1;
        }

        from = msg->from;
        len = ((struct to_body*)from->parsed)->uri.len;
        if (len > EXPRESSION_LENGTH) {
                LOG(L_ERR, "check_refer_to(): From header field is too long: %d chars\n", len);
                return -1;
        }
        strncpy(from_str, ((struct to_body*)from->parsed)->uri.s, len);
        from_str[len] = '\0';

        /* looking for REFER-TO HF */
        if ((!msg->refer_to) && (parse_headers(msg, HDR_REFER_TO_F, 0) == -1)){
                LOG(L_ERR, "check_refer_to(): Error while parsing message\n");
                return -1;
        }

        if (!msg->refer_to) {
                LOG(L_ERR, "check_refer_to(): Refer-To header field not found\n");
                return -1;
        }

        /* we must call parse_refer_to_header explicitly */
        if ((!(msg->refer_to)->parsed) && (parse_refer_to_header(msg) < 0)) {
                LOG(L_ERR, "check_refer_to(): Error while parsing Refer-To body\n");
                return -1;
        }

        refer_to = msg->refer_to;
        len = ((struct to_body*)refer_to->parsed)->uri.len;
        if (len > EXPRESSION_LENGTH) {
                LOG(L_ERR, "check_refer_to(): Refer-To header field is too long: %d chars\n", len);
                return -1;
        }
        strncpy(refer_to_str, ((struct to_body*)refer_to->parsed)->uri.s, len);
        refer_to_str[len] = '\0';

        DBG("check_refer_to(): looking for From: %s Refer-To: %s\n", from_str, refer_to_str);
             /* rule exists in allow file */
        if (search_rule(allow[idx].rules, from_str, refer_to_str)) {
                DBG("check_refer_to(): allow rule found => refer is allowed\n");
                return 1;
        }

        /* rule exists in deny file */
        if (search_rule(deny[idx].rules, from_str, refer_to_str)) {
                DBG("check_refer_to(): deny rule found => refer is denied\n");
                return -1;
        }

        DBG("check_refer_to(): Neither allow nor deny rule found => refer_to is allowed\n");

        return 1;
}
