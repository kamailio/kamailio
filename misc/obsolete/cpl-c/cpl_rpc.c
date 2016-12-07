/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

#include <stdio.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "cpl_db.h"
#include "cpl_parser.h"
#include "cpl_loader.h"
#include "cpl_rpc.h"


static inline int check_userhost( char *p, char *end)
{
	char *p1;
	int  dot;

	/* parse user name */
	p1 = p;
	while (p<end && (isalnum((int)*p) || *p=='-' || *p=='_' || *p=='.' ))
		p++;
	if (p==p1 || p==end || *p!='@')
		return -1;
	p++;
	/* parse the host part */
	dot = 1;
	p1 = p;
	while (p<end) {
		if (*p=='.') {
			if (dot) return -1; /* dot after dot */
			dot = 1;
		} else if (isalnum((int)*p) || *p=='-' || *p=='_' ) {
			dot = 0;
		} else {
			return -1;
		}
		p++;
	}
	if (p1==p || dot)
		return -1;

	return 0;
}


static const char* cpl_load_doc[] = {
	"Load a CPL script to the server.", /* Documentation string */
	0                                   /* Method signature(s) */
};


static void cpl_load(rpc_t* rpc, void* c)
{
	char* cpl_file;
	int cpl_file_len;
	str user;
	str xml = STR_NULL;
	str bin = STR_NULL;
	str enc_log = STR_NULL;

	DBG("DEBUG:cpl-c:cpl_load: \"LOAD_CPL\" FIFO command received!\n");

	if (rpc->scan(c, "s", &user.s) < 1) {
		rpc->fault(c, 400, "Username parameter not found");
		return;
	}
	user.len = strlen(user.s);

	DBG("DEBUG:cpl_load: user=%.*s\n", user.len, user.s);

	if (rpc->scan(c, "s", &cpl_file) < 1) {
		rpc->fault(c, 400, "CPL file name expected");
		return;
	}
	cpl_file_len = strlen(cpl_file);
	DBG("DEBUG:cpl-c:cpl_load: cpl file=%s\n", cpl_file);

	     /* check user+host */
	if (check_userhost( user.s, user.s+user.len)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: invalid user@host [%.*s]\n",
		    user.len,user.s);
		rpc->fault(c, 400, "Bad user@host: %.*s", user.len, user.s);
		return;
	}

	/* load the xml file - this function will allocated a buff for the loading
	 * the cpl file and attach it to xml.s -> don't forget to free it! */
	if (load_file( cpl_file, &xml)!=1) {
		rpc->fault(c, 400, "Cannot read CPL file\n");
		goto error;
	}

	/* get the binary coding for the XML file */
	if (encodeCPL( &xml, &bin, &enc_log)!=1) {
		rpc->fault(c, 400, "%.*s", enc_log.len, enc_log.s);
		goto error;
	}

	/* write both the XML and binary formats into database */
	if (write_to_db(user.s, &xml, &bin)!=1) {
		rpc->fault(c, 400, "Cannot save CPL to database");
		goto error;
	}

	/* free the memory used for storing the cpl script in XML format */
	pkg_free( xml.s );

	/* everything was OK -> dump the logs into response file */
	rpc->add(c, "S", &enc_log);
	if (enc_log.s) pkg_free ( enc_log.s );
	return;
error:
	if (enc_log.s) pkg_free ( enc_log.s );
	if (xml.s) pkg_free ( xml.s );
}



static const char* cpl_remove_doc[] = {
	"Remove a CPL script from server.", /* Documentation string */
	0                                   /* Method signature(s) */
};

static void cpl_remove(rpc_t* rpc, void* c)
{
	char* user;
	int user_len;

	DBG("DEBUG:cpl-c:cpl_remove: \"REMOVE_CPL\" FIFO command received!\n");

	if (rpc->scan(c, "s", &user) < 1) {
		rpc->fault(c, 400, "Username parameter not found");
		return;
	}
	user_len = strlen(user);

	/* check user+host */
	if (check_userhost( user, user+user_len)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_remove: invalid user@host [%.*s]\n",
		    user_len,user);
		rpc->fault(c, 400, "Bad user@host: '%s'", user);
		return;
	}
	
	if (rmv_from_db(user)!=1) {
		rpc->fault(c, 400, "Error while removing CPL script of %s from database", user);
		return;
	}
}


static const char* cpl_get_doc[] = {
	"Return a CPL script.",      /* Documentation string */
	0                          /* Method signature(s) */
};

static void cpl_get(rpc_t* rpc, void* c)
{
	str user;
	str script = STR_NULL;

	if (rpc->scan(c, "S", &user) < 1) {
		rpc->fault(c, 400, "Username parameter expected");
		return;
	}

	DBG("DEBUG:cpl-c:cpl_get: user=%.*s\n", user.len, user.s);

	     /* check user+host */
	if (check_userhost( user.s, user.s+user.len)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: invalid user@host [%.*s]\n",
			user.len,user.s);
		rpc->fault(c, 400, "Bad user@host '%.*s'", user.len, user.s);
		return;
	}
	
	     /* get the script for this user */
	if (get_user_script(&user, &script, 0)==-1) {
		rpc->fault(c, 500, "Database query failed");
		return;
	}

	rpc->add(c, "S", &script);
	if (script.s) shm_free( script.s );
}


/* 
 * RPC Methods exported by this module 
 */
rpc_export_t cpl_rpc_methods[] = {
	{"cpl.load",   cpl_load,   cpl_load_doc,   0},
	{"cpl.remove", cpl_remove, cpl_remove_doc, 0},
	{"cpl.get",    cpl_get,    cpl_get_doc,    0},
	{0, 0, 0, 0}
};
