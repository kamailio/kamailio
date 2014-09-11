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

#ifndef _ALLOW_FILES_H
#define _ALLOW_FILES_H	1

/*
 * loads a config file into the array
 * return value:
 *   <0:  error
 *   >=0: index of the file
 *
 * _def must be true in case of the default file which
 * sets the index to 0
 */
int load_file(char *_name, rule_file_t **_table, int *_rules_num, int _def);

/* free memory allocated for the file container */
void delete_files(rule_file_t **_table, int _num);

/*
 * determines the permission of the call
 * return values:
 * -1:	deny
 * 1:	allow
 */
int check_routing(struct sip_msg* msg, int idx);

/*
 * Test of REGISTER messages. Creates To-Contact pairs and compares them
 * against rules in allow and deny files passed as parameters. The function
 * iterates over all Contacts and creates a pair with To for each contact
 * found. That allows to restrict what IPs may be used in registrations, for
 * example
 */
int check_register(struct sip_msg* msg, int idx);

/*
 * determines the permission to refer to given refer-to uri
 * return values:
 * -1:	deny
 * 1:	allow
 */
int check_refer_to(struct sip_msg* msg, int idx);

#endif /* _ALLOW_FILES_H */
