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
 * Inmplementation of flatstore commands.
 */

#include "flat_cmd.h"
#include "flat_con.h"
#include "flatstore_mod.h"

#include "../../mem/mem.h"

#include <string.h>
#include <errno.h>


/** Destroys a flat_cmd structure.
 * This function frees all memory used by flat_cmd structure.
 * @param cmd A pointer to generic db_cmd command being freed.
 * @param payload A pointer to flat_cmd structure to be freed.
 */
static void flat_cmd_free(db_cmd_t* cmd, struct flat_cmd* payload)
{
	db_drv_free(&payload->gen);
	pkg_free(payload);
}


int flat_cmd(db_cmd_t* cmd)
{
	struct flat_cmd* fcmd;
	db_con_t* con;

	if (cmd->type != DB_PUT) {
		ERR("flatstore: The driver supports PUT operation only.\n");
		return -1;
	}

	if (DB_FLD_EMPTY(cmd->vals)) {
		ERR("flatstore: PUT command with no values encountered\n");
		return -1;
	}

	fcmd = (struct flat_cmd*)pkg_malloc(sizeof(struct flat_cmd));
	if (fcmd == NULL) {
		ERR("flatstore: No memory left\n");
		return -1;
	}
	memset(fcmd, '\0', sizeof(struct flat_cmd));
	if (db_drv_init(&fcmd->gen, flat_cmd_free) < 0) goto error;

	/* FIXME */
	con = cmd->ctx->con[db_payload_idx];
	if (flat_open_table(&fcmd->file_index, con, &cmd->table) < 0) goto error;

	DB_SET_PAYLOAD(cmd, fcmd);
	return 0;

 error:
	if (fcmd) {
		DB_SET_PAYLOAD(cmd, NULL);
		db_drv_free(&fcmd->gen);
		pkg_free(fcmd);
	}
	return -1;
}


int flat_put(db_res_t* res, db_cmd_t* cmd)
{
	struct flat_cmd* fcmd;
	struct flat_con* fcon;
	db_con_t* con;
	int i;
	FILE* f;
	char delims[4], *s;
	size_t len;

	fcmd = DB_GET_PAYLOAD(cmd);
	/* FIXME */
	con = cmd->ctx->con[db_payload_idx];
	fcon = DB_GET_PAYLOAD(con);

	f = fcon->file[fcmd->file_index].f;
	if (f == NULL) {
		ERR("flatstore: Cannot write, file handle not open\n");
		return -1;
	}

	if (flat_local_timestamp < *flat_rotate) {
		flat_con_disconnect(con);
		if (flat_con_connect(con) < 0) {
			ERR("flatstore: Error while rotating files\n");
			return -1;
		}
		flat_local_timestamp = *flat_rotate;
	}

	for(i = 0; !DB_FLD_EMPTY(cmd->vals) && !DB_FLD_LAST(cmd->vals[i]); i++) {
		if (i) {
			if (fprintf(f, "%c", flat_delimiter.s[0]) < 0) goto error;
		}

		/* TODO: how to distinguish NULL from empty */
		if (cmd->vals[i].flags & DB_NULL) continue;
		
		switch(cmd->vals[i].type) {
		case DB_INT:
			if (fprintf(f, "%d", cmd->vals[i].v.int4) < 0) goto error;
			break;

		case DB_FLOAT:
			if (fprintf(f, "%f", cmd->vals[i].v.flt) < 0) goto error;
			break;

		case DB_DOUBLE:
			if (fprintf(f, "%f", cmd->vals[i].v.dbl) < 0) goto error;
			break;

		case DB_DATETIME:
			if (fprintf(f, "%u", (unsigned int)cmd->vals[i].v.time) < 0) 
				goto error;
			break;

		case DB_CSTR:
			s = cmd->vals[i].v.cstr;
			delims[0] = flat_delimiter.s[0];
			delims[1] = flat_record_delimiter.s[0];
			delims[2] = flat_escape.s[0];
			delims[3] = '\0';
			while (*s) {
				len = strcspn(s, delims);
				if (fprintf(f, "%.*s", (int)len, s) < 0) goto error;
				s += len;
				if (*s) {
					/* FIXME: do not use the escaped value for easier parsing */
					if (fprintf(f, "%c%c", flat_escape.s[0], *s) < 0) goto error;
					s++;
				}
			}
			break;

		case DB_STR:
		case DB_BLOB:
			/* FIXME: rewrite */
			s = cmd->vals[i].v.lstr.s;
			len = cmd->vals[i].v.lstr.len;
			while (len > 0) {
				char *c;
				for (c = s; len > 0 && 
						 *c != flat_delimiter.s[0] && 
						 *c != flat_record_delimiter.s[0] && 
						 *c != flat_escape.s[0]; 
					 c++, len--);
				if (fprintf(f, "%.*s", (int)(c-s), s) < 0) goto error;
				s = c;
				if (len > 0) {
					if (fprintf(f, "%c%c", flat_escape.s[0], *s) < 0) goto error;
					s++;
					len--;
				}
			}
			break;

		case DB_BITMAP:
			if (fprintf(f, "%u", cmd->vals[i].v.bitmap) < 0) goto error;
			break;

		default:
			BUG("flatstore: Unsupported field type %d\n", cmd->vals[i].type);
			return -1;
		}
	}

	if (fprintf(f, "%c", flat_record_delimiter.s[0]) < 0) goto error;

	if (flat_flush && (fflush(f) != 0)) {
		ERR("flatstore: Error while flushing file: %s\n", strerror(errno));
		return -1;
	}

	return 0;

 error:
	ERR("flastore: Error while writing data to file\n");
	return -1;
}


/** @} */
