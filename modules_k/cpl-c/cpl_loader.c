/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice-Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "../../fifo_server.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_uri.h"
#include "../../mi/mi.h"
#include "cpl_db.h"
#include "cpl_env.h"
#include "cpl_parser.h"
#include "cpl_loader.h"


#define MAX_STATIC_BUF 256

extern db_con_t* db_hdl;
extern struct cpl_enviroment cpl_env;


#if 0
/* debug function -> write into a file the content of a str struct. */
int write_to_file(char *filename, str *buf)
{
	int fd;
	int ret;

	fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (!fd) {
		LOG(L_ERR,"ERROR:cpl-c:write_to_file: cannot open file : %s\n",
			strerror(errno));
		goto error;
	}

	while ( (ret=write( fd, buf->s, buf->len))!=buf->len) {
		if ((ret==-1 && errno!=EINTR)|| ret!=-1) {
			LOG(L_ERR,"ERROR:cpl-c:write_to_file:cannot write to file:"
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
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot open file for reading:"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get the file length */
	if ( (xml->len=lseek(fd,0,SEEK_END))==-1) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot get file length (lseek):"
			" %s\n", strerror(errno));
		goto error;
	}
	DBG("DEBUG:cpl-c:load_file: file size = %d\n",xml->len);
	if ( lseek(fd,0,SEEK_SET)==-1 ) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: cannot go to beginning (lseek):"
			" %s\n",strerror(errno));
		goto error;
	}

	/* get some memory */
	xml->s = (char*)pkg_malloc( xml->len+1/*null terminated*/ );
	if (!xml->s) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: no more free pkg memory\n");
		goto error;
	}

	/*start reading */
	offset = 0;
	while ( offset<xml->len ) {
		n=read( fd, xml->s+offset, xml->len-offset);
		if (n==-1) {
			if (errno!=EINTR) {
				LOG(L_ERR,"ERROR:cpl-c:load_file: read failed:"
					" %s\n", strerror(errno));
				goto error;
			}
		} else {
			if (n==0) break;
			offset += n;
		}
	}
	if (xml->len!=offset) {
		LOG(L_ERR,"ERROR:cpl-c:load_file: couldn't read all file!\n");
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
		LOG(L_ERR,"ERROR:cpl-c:write_to_file: cannot open response file "
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
				LOG(L_ERR,"ERROR:cpl-c:write_logs_to_file: writev failed: "
					"%s\n", strerror(errno) );
			}
		}
	}

	/* close the file*/
	close( fd );
	return;
}



/* Triggered by fifo server -> implements LOAD_CPL command
 * Command format:
 * -----------------------
 *   :LOAD_CPL:
 *   username
 *   cpl_filename
 *   <empty line>
 * -----------------------
 * For the given user, loads the XML cpl file, compile it into binary format
 * and store both format into database
 */
#define FILE_LOAD_ERR "Error: Cannot read CPL file.\n"
#define DB_SAVE_ERR   "Error: Cannot save CPL to database.\n"
#define USRHOST_ERR   "Error: Bad user@host.\n"
int cpl_load( FILE *fifo_stream, char *response_file )
{
	static char user[MAX_STATIC_BUF];
	static char cpl_file[MAX_STATIC_BUF];
	struct sip_uri uri;
	int user_len;
	int cpl_file_len;
	str xml = {0,0};
	str bin = {0,0};
	str enc_log = {0,0};
	str logs[2];

	DBG("DEBUG:cpl-c:cpl_load: \"LOAD_CPL\" FIFO command received!\n");

	/* check the name of the response file */
	if (response_file==0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: no reply file received from "
			"FIFO command\n");
		goto error;
	}

	/* first line must be the user */
	if (read_line( user, MAX_STATIC_BUF-1 , fifo_stream, &user_len )!=1 ||
	user_len<=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: unable to read username from "
			"FIFO command\n");
		goto error;
	}

	/* second line must be the cpl file */
	if (read_line( cpl_file, MAX_STATIC_BUF-1,fifo_stream,&cpl_file_len)!=1 ||
	cpl_file_len<=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: unable to read cpl_file name from "
			"FIFO command\n");
		goto error;
	}
	cpl_file[cpl_file_len] = 0;

	/* check user+host */
	if (parse_uri( user, user_len, &uri)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: invalid sip URI [%.*s]\n",
			user_len,user);
		logs[1].s = USRHOST_ERR;
		logs[1].len = strlen( USRHOST_ERR );
		goto error1;
	}
	DBG("DEBUG:cpl_load: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* load the xml file - this function will allocated a buff for the loading
	 * the cpl file and attach it to xml.s -> don't forget to free it! */
	if (load_file( cpl_file, &xml)!=1) {
		logs[1].s = FILE_LOAD_ERR;
		logs[1].len = strlen( FILE_LOAD_ERR );
		goto error1;
	}
	DBG("DEBUG:cpl-c:cpl_load: cpl file=%.*s loaded\n",cpl_file_len,cpl_file);

	/* get the binary coding for the XML file */
	if (encodeCPL( &xml, &bin, &enc_log)!=1) {
		logs[1] = enc_log;
		goto error1;
	}
	logs[1] = enc_log;

	/* write both the XML and binary formats into database */
	if (write_to_db( &uri.user, cpl_env.use_domain?&uri.host:0, &xml, &bin)!=1){
		logs[1].s = DB_SAVE_ERR;
		logs[1].len = strlen( DB_SAVE_ERR );
		goto error1;
	}

	/* free the memory used for storing the cpl script in XML format */
	pkg_free( xml.s );

	/* everything was OK -> dump the logs into response file */
	logs[0].s = "OK\n";
	logs[0].len = 3;
	write_to_file( response_file, logs, 2);
	if (enc_log.s) pkg_free ( enc_log.s );
	return 1;
error1:
	logs[0].s = "ERROR\n";
	logs[0].len = 6;
	write_to_file( response_file, logs, 2);
	if (enc_log.s) pkg_free ( enc_log.s );
	if (xml.s) pkg_free ( xml.s );
error:
	return -1;
}



/* Triggered by fifo server -> implements REMOVE_CPL command
 * Command format:
 * -----------------------
 *   :REMOVE_CPL:
 *   username
 *   <empty line>
 * -----------------------
 * For the given user, remove the entire database record
 * (XML cpl and binary cpl); user with empty cpl scripts are not accepted
 */
#define DB_RMV_ERR   "Error: Database remove failed.\n"
int cpl_remove( FILE *fifo_stream, char *response_file )
{
	static char user[MAX_STATIC_BUF];
	struct sip_uri uri;
	int user_len;
	str logs[2];

	DBG("DEBUG:cpl-c:cpl_remove: \"REMOVE_CPL\" FIFO command received!\n");

	/* check the name of the response file */
	if (response_file==0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_remove: no reply file received from "
			"FIFO command\n");
		goto error;
	}

	/* first line must be the username */
	if (read_line( user, MAX_STATIC_BUF-1 , fifo_stream, &user_len )!=1 ||
	user_len<=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_remove: unable to read username from "
			"FIFO command\n");
		goto error;
	}

	/* check user+host */
	if (parse_uri( user, user_len, &uri)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_remove: invalid SIP uri [%.*s]\n",
			user_len,user);
		logs[1].s = USRHOST_ERR;
		logs[1].len = strlen( USRHOST_ERR );
		goto error1;
	}
	DBG("DEBUG:cpl_remove: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	if (rmv_from_db( &uri.user, cpl_env.use_domain?&uri.host:0)!=1) {
		logs[1].s = DB_RMV_ERR;
		logs[1].len = sizeof(DB_RMV_ERR);
		goto error1;
	}

	logs[0].s = "OK\n";
	logs[0].len = 3;
	write_to_file( response_file, logs, 1);
	return 1;
error1:
	logs[0].s = "ERROR\n";
	logs[0].len = 6;
	write_to_file( response_file, logs, 2);
error:
	return -1;
}



/* Triggered by fifo server -> implements GET_CPL command
 * Command format:
 * -----------------------
 *   :GET_CPL:
 *   username
 *   <empty line>
 * -----------------------
 * For the given user, return the CPL script in XML format
 */
#define DB_GET_ERR   "Error: Database query failed.\n"
int cpl_get( FILE *fifo_stream, char *response_file )
{
	static char user[MAX_STATIC_BUF];
	struct sip_uri uri;
	int user_len;
	str script = {0,0};
	str logs[2];

	/* check the name of the response file */
	if (response_file==0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_get: no reply file received from "
			"FIFO command\n");
		goto error;
	}

	/* first line must be the username */
	if (read_line( user, MAX_STATIC_BUF-1 , fifo_stream, &user_len )!=1 ||
	user_len<=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_get: unable to read username from "
			"FIFO command\n");
		goto error;
	}

	/* check user+host */
	if (parse_uri( user, user_len, &uri)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:cpl_load: invalid user@host [%.*s]\n",
			user_len,user);
		logs[1].s = USRHOST_ERR;
		logs[1].len = strlen( USRHOST_ERR );
		goto error1;
	}
	DBG("DEBUG:cpl_get: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* get the script for this user */
	if (get_user_script( &uri.user, cpl_env.use_domain?&uri.host:0,
	&script, "cpl_xml")==-1) {
		logs[1].s = DB_GET_ERR;
		logs[1].len = strlen( DB_GET_ERR );
		goto error1;
	}

	/* write the response into response file - even if script is null */
	write_to_file( response_file, &script, !(script.len==0) );

	if (script.s) shm_free( script.s );

	return 1;
error1:
	logs[0].s = "ERROR\n";
	logs[0].len = 6;
	write_to_file( response_file, logs, 2);
error:
	return -1;
}

#undef MAX_STATIC_BUF

/**************************** MI ****************************/
#define BAD_PARAM_ERR_S   "400 Too few or too many arguments"
#define BAD_PARAM_ERR_LEN (sizeof(BAD_PARAM_ERR_S)-1)
#define FILE_LOAD_ERR_S   "500 Cannot read CPL file"
#define FILE_LOAD_ERR_LEN (sizeof(FILE_LOAD_ERR_S)-1)
#define DB_SAVE_ERR_S     "500 Cannot save CPL to database"
#define DB_SAVE_ERR_LEN   (sizeof(DB_SAVE_ERR_S)-1)
#define CPLFILE_ERR_S     "400 Bad CPL file"
#define CPLFILE_ERR_LEN   (sizeof(CPLFILE_ERR_S)-1)
#define USRHOST_ERR_S     "400 Bad user@host"
#define USRHOST_ERR_LEN   (sizeof(USRHOST_ERR_S)-1)
#define DB_RMV_ERR_S      "500 Database remove failed"
#define DB_RMV_ERR_LEN    (sizeof(DB_RMV_ERR_S)-1)
#define DB_GET_ERR_S      "500 Database query failed"
#define DB_GET_ERR_LEN    (sizeof(DB_GET_ERR_S)-1)

struct mi_node* mi_cpl_load(struct mi_node *cmd, void *param)
{
	struct mi_node *rpl;
	struct sip_uri uri;
	str xml = {0,0};
	str bin = {0,0};
	str enc_log = {0,0};
	str val;
	char *file;

	DBG("DEBUG:cpl-c:mi_cpl_load: \"LOAD_CPL\" FIFO command received!\n");

	/* check user+host */
	if((cmd->kids==NULL) ||(cmd->kids->next==NULL) || (cmd->kids->next->next))
		return init_mi_tree( BAD_PARAM_ERR_S, BAD_PARAM_ERR_LEN);

	val = cmd->kids->value;
	if (parse_uri( val.s, val.len, &uri)!=0){
		LOG(L_ERR,"ERROR:cpl-c:mi_cpl_load: invalid sip URI [%.*s]\n",
			val.len, val.s);
		return init_mi_tree( USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	DBG("DEBUG:cpl-c:mi_cpl_load: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* second argument is the cpl file */
	val = cmd->kids->next->value;
	file = pkg_malloc(val.len+1);
	if (file==NULL) {
		LOG(L_ERR,"ERROR:cpl-c:mi_cpl_load: no more pkg mem\n");
		return 0;
	}
	memcpy( file, val.s, val.len);
	file[val.len]= '\0';

	/* load the xml file - this function will allocated a buff for the loading
	 * the cpl file and attach it to xml.s -> don't forget to free it! */
	if (load_file( file, &xml)!=1) {
		pkg_free(file);
		return init_mi_tree( FILE_LOAD_ERR_S, FILE_LOAD_ERR_LEN );
	}
	DBG("DEBUG:cpl-c:mi_cpl_load: cpl file=%s loaded\n",file);
	pkg_free(file);

	/* get the binary coding for the XML file */
	if (encodeCPL( &xml, &bin, &enc_log)!=1) {
		rpl = init_mi_tree( CPLFILE_ERR_S, CPLFILE_ERR_LEN );
		goto error;
	}

	/* write both the XML and binary formats into database */
	if (write_to_db( &uri.user,cpl_env.use_domain?&uri.host:0, &xml, &bin)!=1){
		rpl = init_mi_tree( DB_SAVE_ERR_S, DB_SAVE_ERR_LEN );
		goto error;
	}

	/* everything was OK */
	rpl = init_mi_tree( MI_200_OK_S, MI_200_OK_LEN);

error:
	if (rpl && enc_log.len)
		add_mi_node_child(rpl,MI_DUP_VALUE,"Log",3,enc_log.s,enc_log.len);
	if (enc_log.s)
		pkg_free ( enc_log.s );
	if (xml.s)
		pkg_free ( xml.s );
	return rpl;
}



struct mi_node * mi_cpl_remove(struct mi_node *cmd, void *param)
{
	struct sip_uri uri;
	str user;

	DBG("DEBUG:cpl-c:mi_cpl_remove: \"REMOVE_CPL\" FIFO command received!\n");

	/* check if there is only one parameter*/
	if(!(cmd->kids && cmd->kids->next== NULL))
		return init_mi_tree( BAD_PARAM_ERR_S, BAD_PARAM_ERR_LEN);

	user = cmd->kids->value;

	/* check user+host */
	if (parse_uri( user.s, user.len, &uri)!=0){
		LOG(L_ERR,"ERROR:cpl-c:mi_cpl_remove: invalid SIP uri [%.*s]\n",
			user.len,user.s);
		return init_mi_tree( USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	DBG("DEBUG:mi_cpl_remove: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	if (rmv_from_db( &uri.user, cpl_env.use_domain?&uri.host:0)!=1)
		return init_mi_tree( DB_RMV_ERR_S, DB_RMV_ERR_LEN );

	return init_mi_tree(MI_200_OK_S, MI_200_OK_LEN);
}



struct mi_node * mi_cpl_get(struct mi_node *cmd, void *param)
{
	struct sip_uri uri;
	struct mi_node* rpl;
	str script = {0,0};
	str user;

	/* check if there is only one parameter*/
	if(!(cmd->kids && cmd->kids->next== NULL))
		return init_mi_tree( BAD_PARAM_ERR_S, BAD_PARAM_ERR_LEN);

	/* check user+host */
	user = cmd->kids->value;
	if (parse_uri( user.s, user.len, &uri)!=0) {
		LOG(L_ERR,"ERROR:cpl-c:mi_cpl_load: invalid user@host [%.*s]\n",
			user.len,user.s);
		return init_mi_tree( USRHOST_ERR_S, USRHOST_ERR_LEN );
	}
	DBG("DEBUG:mi_cpl_get: user@host=%.*s@%.*s\n",
		uri.user.len,uri.user.s,uri.host.len,uri.host.s);

	/* get the script for this user */
	if (get_user_script( &uri.user, cpl_env.use_domain?&uri.host:0,
	&script, "cpl_xml")==-1)
		return init_mi_tree( DB_GET_ERR_S, DB_GET_ERR_LEN );

	/* write the response into response file - even if script is null */
	rpl = init_mi_tree( MI_200_OK_S, MI_200_OK_LEN);
	if (rpl!=NULL)
		add_mi_node_child(rpl, MI_DUP_VALUE, 0, 0, script.s, script.len);

	if (script.s)
		shm_free( script.s );

	return rpl;
}
