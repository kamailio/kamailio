/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice-Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
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
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "cpl_db.h"
#include "cpl_env.h"
#include "cpl_parser.h"
#include "cpl_loader.h"


extern db1_con_t *db_hdl;

#if 0
/* debug function -> write into a file the content of a str struct. */
int write_to_file(char *filename, str *buf)
{
	int fd;
	int ret;

	fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (!fd) {
		LM_ERR("cannot open file : %s\n",
			strerror(errno));
		goto error;
	}

	while ( (ret=write( fd, buf->s, buf->len))!=buf->len) {
		if ((ret==-1 && errno!=EINTR)|| ret!=-1) {
			LM_ERR("cannot write to file:"
				"%s write_ret=%d\n",strerror(errno), ret );
			goto error;
		}
	}
	close(fd);

	return 0;
error:
	return -1;
}
#endif


/* Loads a file into a buffer; first the file length will be determined for
 * allocated an exact buffer len for storing the file content into.
 * Returns:  1 - success
 *          -1 - error
 */
int load_file(char *filename, str *xml)
{
	int n;
	int offset;
	int fd;

	xml->s = 0;
	xml->len = 0;

	/* open the file for reading */
	fd = open(filename, O_RDONLY);
	if(fd == -1) {
		LM_ERR("cannot open file for reading:"
			   " %s\n",
				strerror(errno));
		goto error;
	}

	/* get the file length */
	if((xml->len = lseek(fd, 0, SEEK_END)) == -1) {
		LM_ERR("cannot get file length (lseek):"
			   " %s\n",
				strerror(errno));
		goto error;
	}
	LM_DBG("file size = %d\n", xml->len);
	if(lseek(fd, 0, SEEK_SET) == -1) {
		LM_ERR("cannot go to beginning (lseek):"
			   " %s\n",
				strerror(errno));
		goto error;
	}

	/* get some memory */
	xml->s = (char *)pkg_malloc(xml->len + 1 /*null terminated*/);
	if(!xml->s) {
		PKG_MEM_ERROR;
		goto error;
	}

	/*start reading */
	offset = 0;
	while(offset < xml->len) {
		n = read(fd, xml->s + offset, xml->len - offset);
		if(n == -1) {
			if(errno != EINTR) {
				LM_ERR("read failed:"
					   " %s\n",
						strerror(errno));
				goto error;
			}
		} else {
			if(n == 0)
				break;
			offset += n;
		}
	}
	if(xml->len != offset) {
		LM_ERR("couldn't read all file!\n");
		goto error;
	}
	xml->s[xml->len] = 0;

	close(fd);
	return 1;
error:
	if(fd != -1)
		close(fd);
	if(xml->s)
		pkg_free(xml->s);
	return -1;
}


/* Writes an array of texts into the given response file.
 * Accepts also empty texts, case in which it will be created an empty
 * response file.
 */
void write_to_file(char *file, str *txt, int n)
{
	int fd;

	/* open file for write */
	fd = open(file, O_WRONLY | O_CREAT | O_TRUNC /*|O_NOFOLLOW*/, 0600);
	if(fd == -1) {
		LM_ERR("cannot open response file "
			   "<%s>: %s\n",
				file, strerror(errno));
		return;
	}

	/* write the txt, if any */
	if(n > 0) {
	again:
		if(writev(fd, (struct iovec *)txt, n) == -1) {
			if(errno == EINTR) {
				goto again;
			} else {
				LM_ERR("write_logs_to_file: writev failed: "
					   "%s\n",
						strerror(errno));
			}
		}
	}

	/* close the file*/
	close(fd);
	return;
}


/**************************** RPC ****************************/
static void cpl_rpc_load(rpc_t *rpc, void *ctx)
{
	struct sip_uri uri;
	str xml = {0, 0};
	str bin = {0, 0};
	str enc_log = {0, 0};
	str val;
	char *file;

	LM_DBG("rpc command received!\n");
	if(rpc->scan(ctx, "S", &val) < 1) {
		rpc->fault(ctx, 500, "No URI");
		return;
	}
	if(parse_uri(val.s, val.len, &uri) != 0) {
		LM_ERR("invalid sip URI [%.*s]\n", val.len, val.s);
		rpc->fault(ctx, 500, "Invalid URI");
		return;
	}
	LM_DBG("user@host=%.*s@%.*s\n", uri.user.len, uri.user.s, uri.host.len,
			uri.host.s);
	if(rpc->scan(ctx, "S", &val) < 1) {
		rpc->fault(ctx, 500, "No CPL file");
		return;
	}
	file = pkg_malloc(val.len + 1);
	if(file == NULL) {
		PKG_MEM_ERROR;
		rpc->fault(ctx, 500, "No memory");
		return;
	}
	memcpy(file, val.s, val.len);
	file[val.len] = '\0';

	/* load the xml file - this function will allocated a buff for the loading
	 * the cpl file and attach it to xml.s -> don't forget to free it! */
	if(load_file(file, &xml) != 1) {
		pkg_free(file);
		rpc->fault(ctx, 500, "Failed loading CPL file");
		return;
	}
	LM_DBG("cpl file=%s loaded\n", file);
	pkg_free(file);

	/* get the binary coding for the XML file */
	if(encodeCPL(&xml, &bin, &enc_log) != 1) {
		rpc->fault(ctx, 500, "Failed encoding CPL");
		goto done;
	}

	/* write both the XML and binary formats into database */
	if(write_to_db(&uri.user, cpl_env.use_domain ? &uri.host : 0, &xml, &bin)
			!= 1) {
		rpc->fault(ctx, 500, "Failed saving CPL");
		goto done;
	} else {
		if(rpc->rpl_printf(ctx, "CPL Enabled") < 0) {
			rpc->fault(ctx, 500, "Server error");
			goto done;
		}
	}

done:
	if(enc_log.s)
		pkg_free(enc_log.s);
	if(xml.s)
		pkg_free(xml.s);
}


static void cpl_rpc_remove(rpc_t *rpc, void *ctx)
{
	struct sip_uri uri;
	str user;

	LM_DBG("rpc command received!\n");
	if(rpc->scan(ctx, "S", &user) < 1) {
		rpc->fault(ctx, 500, "No URI");
		return;
	}

	/* check user+host */
	if(parse_uri(user.s, user.len, &uri) != 0) {
		LM_ERR("invalid SIP uri [%.*s]\n", user.len, user.s);
		rpc->fault(ctx, 500, "Invalid URI");
		return;
	}
	LM_DBG("user@host=%.*s@%.*s\n", uri.user.len, uri.user.s, uri.host.len,
			uri.host.s);

	if(rmv_from_db(&uri.user, cpl_env.use_domain ? &uri.host : 0) != 1) {
		rpc->fault(ctx, 500, "Remove failed");
		return;
	} else {
		if(rpc->rpl_printf(ctx, "CPL Disabled") < 0) {
			rpc->fault(ctx, 500, "Server error");
			return;
		}
	}
}


static void cpl_rpc_get(rpc_t *rpc, void *ctx)
{
	struct sip_uri uri;
	str script = {0, 0};
	str user;

	LM_DBG("rpc command received!\n");
	if(rpc->scan(ctx, "S", &user) < 1) {
		rpc->fault(ctx, 500, "No URI");
		return;
	}

	/* check user+host */
	if(parse_uri(user.s, user.len, &uri) != 0) {
		LM_ERR("invalid SIP uri [%.*s]\n", user.len, user.s);
		rpc->fault(ctx, 500, "Invalid URI");
		return;
	}
	LM_DBG("user@host=%.*s@%.*s\n", uri.user.len, uri.user.s, uri.host.len,
			uri.host.s);

	/* get the script for this user */
	str query_str = str_init("cpl_xml");
	if(get_user_script(&uri.user, cpl_env.use_domain ? &uri.host : 0, &script,
			   &query_str)
			== -1) {
		rpc->fault(ctx, 500, "No CPL script");
		return;
	}

	/* write the cpl script into response */
	if(script.s) {
		if(rpc->add(ctx, "S", &script) < 0) {
			rpc->fault(ctx, 500, "Server error");
			goto done;
		}
	}
done:
	if(script.s)
		shm_free(script.s);
}

static const char *cpl_rpc_load_doc[2] = {"Load cpl for a user.", 0};

static const char *cpl_rpc_remove_doc[2] = {"Remove cpl for a user.", 0};

static const char *cpl_rpc_get_doc[2] = {"Get cpl for a user.", 0};

rpc_export_t cpl_rpc[] = {{"cpl.load", cpl_rpc_load, cpl_rpc_load_doc, 0},
		{"cpl.remove", cpl_rpc_remove, cpl_rpc_remove_doc, 0},
		{"cpl.get", cpl_rpc_get, cpl_rpc_get_doc, 0}, {0, 0, 0, 0}};

int cpl_rpc_init(void)
{
	if(rpc_register_array(cpl_rpc) != 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}
