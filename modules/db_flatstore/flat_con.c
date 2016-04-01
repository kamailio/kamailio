/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
 * Copyright (C) 2008 iptelorg GmbH
 * Written by Jan Janak <jan@iptel.org>
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \addtogroup flatstore
 * @{ 
 */

/** \file 
 * Inmplementation of flatstore "connections".
 */

#include "flat_con.h"
#include "flatstore_mod.h"
#include "flat_uri.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


/** Free all memory allocated for a flat_con structure.
 * This function function frees all memory that is in use by
 * a flat_con structure.
 * @param con A generic db_con connection structure.
 * @param payload Flatstore specific payload to be freed.
 */
static void flat_con_free(db_con_t* con, struct flat_con* payload)
{
	int i;
	if (!payload) return;

	/* delete the structure only if there are no more references
	 * to it in the connection pool
	 */
	if (db_pool_remove((db_pool_entry_t*)payload) == 0) return;
	
	db_pool_entry_free(&payload->gen);

	if (payload->file) {
		for(i = 0; i < payload->n; i++) {
			if (payload->file[i].filename) pkg_free(payload->file[i].filename);
			if (payload->file[i].table.s) pkg_free(payload->file[i].table.s);
			if (payload->file[i].f) fclose(payload->file[i].f);
		}
		pkg_free(payload->file);
	}
	pkg_free(payload);
}


int flat_con(db_con_t* con)
{
	struct flat_con* fcon;

	/* First try to lookup the connection in the connection pool and
	 * re-use it if a match is found
	 */
	fcon = (struct flat_con*)db_pool_get(con->uri);
	if (fcon) {
		DBG("flatstore: A handle to %.*s found in the connection pool\n",
			STR_FMT(&con->uri->body));
		goto found;
	}

	fcon = (struct flat_con*)pkg_malloc(sizeof(struct flat_con));
	if (fcon == NULL) {
		ERR("flatstore: No memory left\n");
		goto error;
	}
	memset(fcon, '\0', sizeof(struct flat_con));
	if (db_pool_entry_init(&fcon->gen, flat_con_free, con->uri) < 0) goto error;

	DBG("flastore: Preparing new file handles to files in %.*s\n", 
		STR_FMT(&con->uri->body));
	
	/* Put the newly created flatstore connection into the pool */
	db_pool_put((struct db_pool_entry*)fcon);
	DBG("flatstore: Handle stored in connection pool\n");

 found:
	/* Attach driver payload to the db_con structure and set connect and
	 * disconnect functions
	 */
	DB_SET_PAYLOAD(con, fcon);
	con->connect = flat_con_connect;
	con->disconnect = flat_con_disconnect;
	return 0;

 error:
	if (fcon) {
		db_pool_entry_free(&fcon->gen);
		pkg_free(fcon);
	}
	return -1;
}


int flat_con_connect(db_con_t* con)
{
	struct flat_con* fcon;
	int i;
	
	fcon = DB_GET_PAYLOAD(con);
	
	/* Do not reconnect already connected connections */
	if (fcon->flags & FLAT_OPENED) return 0;

	DBG("flatstore: Opening handles to files in '%.*s'\n", 
		STR_FMT(&con->uri->body));

	/* FIXME: Make sure the directory exists, is accessible,
	 * and we can create files there
	 */

	DBG("flatstore: Directory '%.*s' opened successfully\n", 
		STR_FMT(&con->uri->body));

	for(i = 0; i < fcon->n; i++) {
		if (fcon->file[i].f) {
			fclose(fcon->file[i].f);
		}
		fcon->file[i].f = fopen(fcon->file[i].filename, "a");
		if (fcon->file[i].f == NULL) {
			ERR("flatstore: Error while opening file handle to '%s': %s\n", 
				fcon->file[i].filename, strerror(errno));
			return -1;
		}
	}

	fcon->flags |= FLAT_OPENED;
	return 0;

}


void flat_con_disconnect(db_con_t* con)
{
	struct flat_con* fcon;
	int i;

	fcon = DB_GET_PAYLOAD(con);

	if ((fcon->flags & FLAT_OPENED) == 0) return;

	DBG("flatstore: Closing handles to files in '%.*s'\n", 
		STR_FMT(&con->uri->body));

	for(i = 0; i < fcon->n; i++) {
		if (fcon->file[i].f == NULL) continue;
		fclose(fcon->file[i].f);
		fcon->file[i].f = NULL;
	}

	fcon->flags &= ~FLAT_OPENED;
}


/* returns a pkg_malloc'ed file name */
static char* get_filename(str* dir, str* name)
{
    char* buf, *p;
    int buf_len, total_len;

    buf_len = pathmax();

    total_len = dir->len + 1 /* / */ + 
		name->len + 1 /* _ */+
		flat_pid.len +
		flat_suffix.len + 1 /* \0 */;

    if (buf_len < total_len) {
        ERR("flatstore: The path is too long (%d and PATHMAX is %d)\n",
            total_len, buf_len);
        return 0;
    }

    if ((buf = pkg_malloc(buf_len)) == NULL) {
        ERR("flatstore: No memory left\n");
        return 0;
    }
    p = buf;

    memcpy(p, dir->s, dir->len);
    p += dir->len;

    *p++ = '/';

    memcpy(p, name->s, name->len);
    p += name->len;

    *p++ = '_';

    memcpy(p, flat_pid.s, flat_pid.len);
    p += flat_pid.len;

    memcpy(p, flat_suffix.s, flat_suffix.len);
    p += flat_suffix.len;

    *p = '\0';
    return buf;
}



int flat_open_table(int* idx, db_con_t* con, str* name)
{
	struct flat_uri* furi;
	struct flat_con* fcon;
	struct flat_file* new;
	int i;
	char* filename, *table;

	new = NULL;
	filename = NULL;
	table = NULL;
	fcon = DB_GET_PAYLOAD(con);
	furi = DB_GET_PAYLOAD(con->uri);
	
	for(i = 0; i < fcon->n; i++) {
		if (name->len == fcon->file[i].table.len &&
			!strncmp(name->s, fcon->file[i].table.s, name->len))
			break;
	}
	if (fcon->n == i) {
		/* Perform operations that can fail first (before resizing
		 * fcon->file, so that we can fail gracefully if one of the
		 * operations fail. 
		 */
		if ((filename = get_filename(&furi->path, name)) == NULL)
			goto no_mem;

		if ((table = pkg_malloc(name->len)) == NULL) goto no_mem;
		memcpy(table, name->s, name->len);

		new = pkg_realloc(fcon->file, sizeof(struct flat_file) * (fcon->n + 1));
		if (new == NULL) goto no_mem;

		fcon->file = new;
		new = new + fcon->n; /* Advance to the new (last) element */
		fcon->n++;

		new->table.s = table;
		new->table.len = name->len;
		new->filename = filename;

		/* Also open the file if we are connected already */
		if (fcon->flags & FLAT_OPENED) {
			if ((new->f = fopen(new->filename, "a")) == NULL) {
				ERR("flatstore: Error while opening file handle to '%s': %s\n", 
					new->filename, strerror(errno));
				return -1;
			}			
		} else {
			new->f = NULL;
		}
		
		*idx = fcon->n - 1;
	} else {
		*idx = i;
	}
	DBG("flatstore: Handle to file '%s' opened successfully\n", 
		fcon->file[*idx].filename);
	return 0;

 no_mem:
	ERR("flatstore: No memory left\n");
	if (filename) pkg_free(filename);
	if (table) pkg_free(table);
	return -1;
}


/** @} */
