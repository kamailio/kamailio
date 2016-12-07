/*
 * $Id$
 *
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
 *
 * History:
 * -------
 * 2003-08-21: cpl_remove() added (bogdan)
 * 2003-06-24: file created (bogdan)
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "../../parser/parse_uri.h"
#include "../../lib/kmi/mi.h"
#include "cpl_db.h"
#include "cpl_env.h"
#include "cpl_parser.h"
#include "cpl_loader.h"


extern db1_con_t* db_hdl;

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
int load_file( char *filename, str *xml)
{
	int n;
	int offset;
	int fd;

	xml->s = 0;
	xml->len = 0;

	/* open the file for reading */
	fd = open(filename,O_RDONLY);
	if (fd==-1) {
		LM_ERR("cannot open file for reading:"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get the file length */
	if ( (xml->len=lseek(fd,0,SEEK_END))==-1) {
		LM_ERR("cannot get file length (lseek):"
			" %s\n", strerror(errno));
		goto error;
	}
	LM_DBG("file size = %d\n",xml->len);
	if ( lseek(fd,0,SEEK_SET)==-1 ) {
		LM_ERR("cannot go to beginning (lseek):"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get some memory */
	xml->s = (char*)pkg_malloc( xml->len+1/*null terminated*/ );
	if (!xml->s) {
		LM_ERR("no more free pkg memory\n");
		goto error;
	}

	/*start reading */
	offset = 0;
	while ( offset<xml->len ) {
		n=read( fd, xml->s+offset, xml->len-offset);
		if (n==-1) {
			if (errno!=EINTR) {
				LM_ERR("read failed:"
					" %s\n", strerror(errno));
				goto error;
			}
		} else {
			if (n==0) break;
			offset += n;
		}
	}
	if (xml->len!=offset) {
		LM_ERR("couldn't read all file!\n");
		goto error;
	}
	xml->s[xml->len] = 0;

	close(fd);
	return 1;
error:
	if (fd!=-1) close(fd);
	if (xml->s) pkg_free( xml->s);
	return -1;
}



/* Writes an array of texts into the given response file.
 * Accepts also empty texts, case in which it will be created an empty
 * response file.
 */
void write_to_file( char *file, str *txt, int n )
{
	int fd;

	/* open file for write */
	fd = open( file, O_WRONLY|O_CREAT|O_TRUNC/*|O_NOFOLLOW*/, 0600 );
	if (fd==-1) {
		LM_ERR("cannot open response file "
			"<%s>: %s\n", file, strerror(errno));
		return;
	}

	/* write the txt, if any */
	if (n>0) {
again:
		if ( writev( fd, (struct iovec*)txt, n)==-1) {
			if (errno==EINTR) {
				goto again;
			} else {
				LM_ERR("write_logs_to_file: writev failed: "
					"%s\n", strerror(errno) );
			}
		}
	}

	/* close the file*/
	close( fd );
	return;
}


/**************************** MI ****************************/
#define FILE_LOAD_ERR_S   "Cannot read CPL file"
#define FILE_LOAD_ERR_LEN (sizeof(FILE_LOAD_ERR_S)-1)
#define DB_SAVE_ERR_S     "Cannot save CPL to database"
#define DB_SAVE_ERR_LEN   (sizeof(DB_SAVE_ERR_S)-1)
#define CPLFILE_ERR_S     "Bad CPL file"
#define CPLFILE_ERR_LEN   (sizeof(CPLFILE_ERR_S)-1)
#define USRHOST_ERR_S     "Bad user@host"
#define USRHOST_ERR_LEN   (sizeof(USRHOST_ERR_S)-1)
#define DB_RMV_ERR_S      "Database remove failed"
#define DB_RMV_ERR_LEN    (sizeof(DB_RMV_ERR_S)-1)
#define DB_GET_ERR_S      "Database query failed"
#define DB_GET_ERR_LEN    (sizeof(DB_GET_ERR_S)-1)

struct mi_root* mi_cpl_load(struct mi_root *cmd_tree, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *cmd;
	struct sip_uri uri;
	str xml = {0,0};
	str bin = {0,0};
	str enc_log = {0,0};
	str val;
	char *file;

	LM_DBG("\"LOAD_CPL\" MI command received!\n");
	cmd = &cmd_tree->node;

	/* check user+host */
	if((cmd->kids==NULL) ||(cmd->kids->next==NULL) || (cmd->kids->next->next))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	val = cmd->kids->value;
	if (parse_uri( val.s, val.len, &uri)!=0){
		LM_ERR("invalid sip URI [%.*s]\n",
			val.len, val.s);
		return init_mi_tree( 400, USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	LM_DBG("user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* second argument is the cpl file */
	val = cmd->kids->next->value;
	file = pkg_malloc(val.len+1);
	if (file==NULL) {
		LM_ERR("no more pkg mem\n");
		return 0;
	}
	memcpy( file, val.s, val.len);
	file[val.len]= '\0';

	/* load the xml file - this function will allocated a buff for the loading
	 * the cpl file and attach it to xml.s -> don't forget to free it! */
	if (load_file( file, &xml)!=1) {
		pkg_free(file);
		return init_mi_tree( 500, FILE_LOAD_ERR_S, FILE_LOAD_ERR_LEN );
	}
	LM_DBG("cpl file=%s loaded\n",file);
	pkg_free(file);

	/* get the binary coding for the XML file */
	if (encodeCPL( &xml, &bin, &enc_log)!=1) {
		rpl_tree = init_mi_tree( 500, CPLFILE_ERR_S, CPLFILE_ERR_LEN );
		goto error;
	}

	/* write both the XML and binary formats into database */
	if (write_to_db( &uri.user,cpl_env.use_domain?&uri.host:0, &xml, &bin)!=1){
		rpl_tree = init_mi_tree( 500, DB_SAVE_ERR_S, DB_SAVE_ERR_LEN );
		goto error;
	}

	/* everything was OK */
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);

error:
	if (rpl_tree && enc_log.len)
		add_mi_node_child(&rpl_tree->node,MI_DUP_VALUE,"Log",3,enc_log.s,enc_log.len);
	if (enc_log.s)
		pkg_free ( enc_log.s );
	if (xml.s)
		pkg_free ( xml.s );
	return rpl_tree;
}



struct mi_root * mi_cpl_remove(struct mi_root *cmd_tree, void *param)
{
	struct mi_node *cmd;
	struct sip_uri uri;
	str user;

	LM_DBG("\"REMOVE_CPL\" MI command received!\n");
	cmd = &cmd_tree->node;

	/* check if there is only one parameter*/
	if(!(cmd->kids && cmd->kids->next== NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	user = cmd->kids->value;

	/* check user+host */
	if (parse_uri( user.s, user.len, &uri)!=0){
		LM_ERR("invalid SIP uri [%.*s]\n",
			user.len,user.s);
		return init_mi_tree( 400, USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	LM_DBG("user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	if (rmv_from_db( &uri.user, cpl_env.use_domain?&uri.host:0)!=1)
		return init_mi_tree( 500, DB_RMV_ERR_S, DB_RMV_ERR_LEN );

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}



struct mi_root * mi_cpl_get(struct mi_root *cmd_tree, void *param)
{
	struct mi_node *cmd;
	struct sip_uri uri;
	struct mi_root* rpl_tree;
	str script = {0,0};
	str user;

	cmd = &cmd_tree->node;

	/* check if there is only one parameter*/
	if(!(cmd->kids && cmd->kids->next== NULL))
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* check user+host */
	user = cmd->kids->value;
	if (parse_uri( user.s, user.len, &uri)!=0) {
		LM_ERR("invalid user@host [%.*s]\n",
			user.len,user.s);
		return init_mi_tree( 400, USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	LM_DBG("user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* get the script for this user */
	str query_str = str_init("cpl_xml");
	if (get_user_script( &uri.user, cpl_env.use_domain?&uri.host:0,
	&script, &query_str)==-1)
		return init_mi_tree( 500, DB_GET_ERR_S, DB_GET_ERR_LEN );

	/* write the response into response file - even if script is null */
	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree!=NULL)
		add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE, 0, 0,
			script.s, script.len);

	if (script.s)
		shm_free( script.s );

	return rpl_tree;
}
