/*
 *
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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


#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include "../../mem/mem.h"
#include "../../error.h"
#include "../../config.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../dset.h"
#include "config.h"

static int set_environment(struct sip_msg *msg)
{
	static char srcip[64];

	if (snprintf(srcip, 64, SRCIP "=%s", ip_addr2a(&msg->src_ip))==-1) {
		LOG(L_ERR, "ERROR: set_environment: spritnf failed\n");
		return 0;
	}
	if (putenv(srcip)==-1) {
		LOG(L_ERR, "ERROR: set_environment: putenv failed\n");
		return 0;
	}
	DBG("DEBUG: environment variable set: %s\n", srcip );
	return 1;
}

int exec_msg(struct sip_msg *msg, char *cmd )
{
	int ret;
	FILE *pipe;
	int exit_status;

#ifdef __EXTRA_DEBUG
	FILE* f;
	printf("\n\n adding TEST\n\n");
	putenv("SRCIPA=192.168.99.100");
	f=popen("printenv > /tmp/xxx", "r");
	pclose(f);
#endif

	ret=-1;
	set_environment(msg);
#ifdef __EXTRA_DEBUG
	putenv("SRCIPB=192.168.99.100");
#endif
	pipe=popen( cmd, "w" );
	if (pipe==NULL) {
		LOG(L_ERR, "ERROR: exec_msg: cannot open pipe: %s\n",
			cmd);
		ser_error=E_EXEC;
		goto error00;
	}

	if (fwrite(msg->orig, 1, msg->len, pipe)!=msg->len) {
		LOG(L_ERR, "ERROR: exec_msg: error writing to pipe\n");
		ser_error=E_EXEC;
		goto error01;
	}
	/* success */
	ret=1;

error01:
	if (ferror(pipe)) {
		LOG(L_ERR, "ERROR: exec_str: error in pipe: %s\n",
			strerror(errno));
		ser_error=E_EXEC;
	}
	exit_status=pclose(pipe);
	if (exit_status!=0) {
		DBG("exec_str: exit_status=%d, errno=%d: %s\n",
			exit_status, errno, strerror(errno) );
	}
error00:
	return ret;
}

int exec_str(struct sip_msg *msg, char *cmd, char *param) {

	int cmd_len;
	FILE *pipe;	
	char *cmd_line;
	int ret;
	int l1, l2;
	char uri_line[MAX_URI_SIZE+1];
	int uri_cnt;
	int uri_len;
	char *new_uri;
	int exit_status;

	/* pesimist: assume error by default */
	ret=-1;

	set_environment(msg);
	l1=strlen(cmd);l2=strlen(param);cmd_len=l1+l2+2;
	cmd_line=pkg_malloc(cmd_len);
	if (cmd_line==0) {
		ret=ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: exec_str: no mem for command\n");
		goto error00;
	}

	/* 'command parameter \0' */
	memcpy(cmd_line, cmd, l1); cmd_line[l1]=' ';
	memcpy(cmd_line+l1+1, param, l2);cmd_line[l1+l2+1]=0;
	
	pipe=popen( cmd_line, "r" );
	if (pipe==NULL) {
		LOG(L_ERR, "ERROR: exec_str: cannot open pipe: %s\n",
			cmd_line);
		ser_error=E_EXEC;
		goto error01;
	}

	/* read now line by line */
	uri_cnt=0;
	while( fgets(uri_line, MAX_URI_SIZE, pipe)!=NULL){
		uri_len=strlen(uri_line);
		/* trim from right */
		while(uri_len && (uri_line[uri_len-1]=='\r' 
				|| uri_line[uri_len-1]=='\n' 
				|| uri_line[uri_len-1]=='\t'
				|| uri_line[uri_len-1]==' ' )) {
			DBG("exec_str: rtrim\n");
			uri_len--;
		}
		/* skip empty line */
		if (uri_len==0) continue;
		/* ZT */
		uri_line[uri_len]=0;
		if (uri_cnt==0) {
			new_uri=pkg_malloc(uri_len);
			if (new_uri==0) {
				LOG(L_ERR, "ERROR: exec_str no uri mem\n");
				ser_error=E_OUT_OF_MEM;
				goto error02;
			}
			memcpy(new_uri, uri_line, uri_len );
			if (msg->new_uri.s) {
				pkg_free(msg->new_uri.s);
			}
			msg->new_uri.s=new_uri;
			msg->new_uri.len=uri_len;
		} else {
			if (append_branch(msg, uri_line, uri_len)==-1) {
				LOG(L_ERR, "ERROR: exec_str: append_branch failed;"
					" too many or too long URIs?\n");
				goto error02;
			}
		}
		uri_cnt++;
	}
	if (uri_cnt==0) {
		LOG(L_ERR, "ERROR:exec_str: no uri from %s\n", cmd_line );
		goto error02;
	}
	/* success */
	ret=1;

error02:
	if (ferror(pipe)) {
		LOG(L_ERR, "ERROR: exec_str: error in pipe: %s\n",
			strerror(errno));
		ser_error=E_EXEC;
	}
	exit_status=pclose(pipe);
	if (exit_status!=0) {
		DBG("exec_str: exit_status=%d, errno=%d: %s\n",
			exit_status, errno, strerror(errno) );
	}
error01:
	pkg_free(cmd_line);
error00:
	return ret;
}
