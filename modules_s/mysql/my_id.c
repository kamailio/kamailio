/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2004 iptel.org
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

#include <stdlib.h>
#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "my_id.h"
#include "utils.h"


/*
 * Create a new connection identifier
 */
struct my_id* new_my_id(const char* url)
{
	char* buf, *username, *password, *host, *port, *database;
	int l;
	struct my_id* ptr;

	if (!url) {
		LOG(L_ERR, "new_my_id(): Invalid parameter\n");
		return 0;
	}

	     /* Make a scratch-pad copy of the url */
	l = strlen(url);
	buf = (char*)pkg_malloc(l + 1);
	if (!buf) {
		LOG(L_ERR, "new_my_id(): Not enough memory\n");
		return 0;
	}
	memcpy(buf, url, l + 1);

	ptr = (struct my_id*)pkg_malloc(sizeof(struct my_id));
	if (!ptr) {
		LOG(L_ERR, "new_my_id(): No memory left\n");
		goto err;
	}
	memset(ptr, 0, sizeof(struct my_id));

	if (parse_mysql_url(buf, &username, &password, &host, &port, &database) < 0) {
		LOG(L_ERR, "new_my_id(): Error while parsing mysql URL: %s\n", url);
		goto err;
	}

	ptr->username.len = strlen(username);
	ptr->username.s = (char*)pkg_malloc(ptr->username.len + 1);
	if (!ptr->username.s) {
		LOG(L_ERR, "new_connection(): No memory left\n");
		goto err;
	}
	memcpy(ptr->username.s, username, ptr->username.len + 1);

	if (password) {
		ptr->password.len = strlen(password);
		ptr->password.s = (char*)pkg_malloc(ptr->password.len + 1);
		if (!ptr->password.s) {
			LOG(L_ERR, "new_connection(): No memory left\n");
			goto err;
		}
		memcpy(ptr->password.s, password, ptr->password.len + 1);
	}

	ptr->host.len = strlen(host);
	ptr->host.s = (char*)pkg_malloc(ptr->host.len + 1);
	if (!ptr->host.s) {
		LOG(L_ERR, "new_connection(): No memory left\n");
		goto err;
	}
	memcpy(ptr->host.s, host, ptr->host.len + 1);

	if (port && *port) {
		ptr->port = atoi(port);
	} else {
		ptr->port = 0;
	}

	ptr->database.len = strlen(database);
	ptr->database.s = (char*)pkg_malloc(ptr->database.len + 1);
	if (!ptr->database.s) {
		LOG(L_ERR, "new_connection(): No memory left\n");
		goto err;
	}
	memcpy(ptr->database.s, database, ptr->database.len + 1);

	pkg_free(buf);
	return ptr;

 err:
	if (buf) pkg_free(buf);
	if (ptr && ptr->username.s) pkg_free(ptr->username.s);
	if (ptr && ptr->password.s) pkg_free(ptr->password.s);
	if (ptr && ptr->host.s) pkg_free(ptr->host.s);
	if (ptr && ptr->database.s) pkg_free(ptr->database.s);
	if (ptr) pkg_free(ptr);
	return 0;
}


/*
 * Compare two connection identifiers
 */
unsigned char cmp_my_id(struct my_id* id1, struct my_id* id2)
{
	if (!id1 || !id2) return 0;
	if (id1->port != id2->port) return 0;
	if (id1->username.len != id2->username.len) return 0;
	if (id1->password.len != id2->password.len) return 0;
	if (id1->host.len != id2->host.len) return 0;
	if (id1->database.len != id2->database.len) return 0;

	if (memcmp(id1->username.s, id2->username.s, id1->username.len)) return 0;
	if (memcmp(id1->password.s, id2->password.s, id1->password.len)) return 0;
	if (strncasecmp(id1->host.s, id2->host.s, id1->host.len)) return 0;
	if (memcmp(id1->database.s, id2->database.s, id1->database.len)) return 0;
	return 1;
}


/*
 * Free a connection identifier
 */
void free_my_id(struct my_id* id)
{
	if (!id) return;

	if (id->username.s) pkg_free(id->username.s);
	if (id->password.s) pkg_free(id->password.s);
	if (id->host.s) pkg_free(id->host.s);
	if (id->database.s) pkg_free(id->database.s);
	pkg_free(id);
}
