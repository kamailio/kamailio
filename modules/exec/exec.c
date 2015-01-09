/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Exec module:: Module interface
 * \ingroup exec
 * Module: \ref exec
 */

/**
 * @defgroup exec Execute external applications
 * @brief Kamailio exec module
 *
 * The exec module allows external commands to be executed from a Kamailio script.
 * The commands may be any valid shell commands--the command string is passed to the 
 * shell using “popen” command. Kamailio passes additional information about the request
 * in environment variables.
 *
 */




#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
/* 
#include <sys/resource.h>
*/
#include <sys/wait.h>
#include "../../mem/mem.h"
#include "../../error.h"
#include "../../config.h"
#include "../../parser/msg_parser.h"
#include "../../dprint.h"
#include "../../dset.h"
#include "../../action.h"
#include "../../usr_avp.h"

#include "exec.h"

int exec_msg(struct sip_msg *msg, char *cmd )
{
	FILE *pipe;
	int exit_status;
	int ret;

	ret=-1; /* pessimist: assume error */
	pipe=popen( cmd, "w" );
	if (pipe==NULL) {
		LM_ERR("cannot open pipe: %s\n", cmd);
		ser_error=E_EXEC;
		return -1;
	}

	if (fwrite(msg->buf, 1, msg->len, pipe)!=msg->len) {
		LM_ERR("failed to write to pipe\n");
		ser_error=E_EXEC;
		goto error01;
	}
	/* success */
	ret=1;

error01:
	if (ferror(pipe)) {
		LM_ERR("pipe: %s\n", strerror(errno));
		ser_error=E_EXEC;
		ret=-1;
	}
	exit_status=pclose(pipe);
	if (WIFEXITED(exit_status)) { /* exited properly .... */
		/* return false if script exited with non-zero status */
		if (WEXITSTATUS(exit_status)!=0) ret=-1;
	} else { /* exited erroneously */
		LM_ERR("cmd %s failed. exit_status=%d, errno=%d: %s\n",
			cmd, exit_status, errno, strerror(errno) );
		ret=-1;
	}
	return ret;
}

int exec_str(struct sip_msg *msg, char *cmd, char *param, int param_len) {

	struct action act;
	struct run_act_ctx ra_ctx;
	int cmd_len;
	FILE *pipe;	
	char *cmd_line;
	int ret;
	int l1;
	static char uri_line[MAX_URI_SIZE+1];
	int uri_cnt;
	str uri;
	int exit_status;

	/* pessimist: assume error by default */
	ret=-1;
	
	l1=strlen(cmd);
	if(param_len>0)
		cmd_len=l1+param_len+4;
	else
		cmd_len=l1+1;
	cmd_line=pkg_malloc(cmd_len);
	if (cmd_line==0) {
		ret=ser_error=E_OUT_OF_MEM;
		LM_ERR("no pkg mem for command\n");
		goto error00;
	}

	/* 'command parameter \0' */
	memcpy(cmd_line, cmd, l1); 
	if(param_len>0)
	{
		cmd_line[l1]=' ';
		cmd_line[l1+1]='\'';
		memcpy(cmd_line+l1+2, param, param_len);
		cmd_line[l1+param_len+2]='\'';
		cmd_line[l1+param_len+3]=0;
	} else {
		cmd_line[l1] = 0;
	}
	
	pipe=popen( cmd_line, "r" );
	if (pipe==NULL) {
		LM_ERR("cannot open pipe: %s\n", cmd_line);
		ser_error=E_EXEC;
		goto error01;
	}

	/* read now line by line */
	uri_cnt=0;
	while( fgets(uri_line, MAX_URI_SIZE, pipe)!=NULL){
		uri.s = uri_line;
		uri.len=strlen(uri.s);
		/* trim from right */
		while(uri.len && (uri.s[uri.len-1]=='\r' 
				|| uri.s[uri.len-1]=='\n' 
				|| uri.s[uri.len-1]=='\t'
				|| uri.s[uri.len-1]==' ' )) {
			LM_DBG("rtrim\n");
			uri.len--;
		}
		/* skip empty line */
		if (uri.len==0) continue;
		/* ZT */
		uri.s[uri.len]=0;
		if (uri_cnt==0) {
			memset(&act, 0, sizeof(act));
			act.type = SET_URI_T;
			act.val[0].type = STRING_ST;
			act.val[0].u.string = uri.s;
			init_run_actions_ctx(&ra_ctx);
			if (do_action(&ra_ctx, &act, msg)<0) {
				LM_ERR("the action for has failed\n");
				ser_error=E_OUT_OF_MEM;
				goto error02;
			}
		} else {
		    if (append_branch(msg, &uri, 0, 0, Q_UNSPECIFIED, 0, 0,
				      0, 0, 0, 0) == -1) {
				LM_ERR("append_branch failed; too many or too long URIs?\n");
				goto error02;
			}
		}
		uri_cnt++;
	}
	if (uri_cnt==0) {
		LM_ERR("no uri from %s\n", cmd_line );
		goto error02;
	}
	/* success */
	ret=1;

error02:
	if (ferror(pipe)) {
		LM_ERR("in pipe: %s\n", strerror(errno));
		ser_error=E_EXEC;
		ret=-1;
	}
	exit_status=pclose(pipe);
	if (WIFEXITED(exit_status)) { /* exited properly .... */
		/* return false if script exited with non-zero status */
		if (WEXITSTATUS(exit_status)!=0) ret=-1;
	} else { /* exited erroneously */
		LM_ERR("cmd %s failed. exit_status=%d, errno=%d: %s\n",
			cmd, exit_status, errno, strerror(errno) );
		ret=-1;
	}
error01:
	pkg_free(cmd_line);
error00:
	return ret;
}


int exec_avp(struct sip_msg *msg, char *cmd, pvname_list_p avpl)
{
	int_str avp_val;
	int_str avp_name;
	unsigned short avp_type;
	FILE *pipe;	
	int ret;
	char res_line[MAX_URI_SIZE+1];
	str res;
	int exit_status;
	int i;
	pvname_list_t* crt;

	/* pessimist: assume error by default */
	ret=-1;
	
	pipe=popen( cmd, "r" );
	if (pipe==NULL) {
		LM_ERR("cannot open pipe: %s\n", cmd);
		ser_error=E_EXEC;
		return ret;
	}

	/* read now line by line */
	i=0;
	crt = avpl;
	while( fgets(res_line, MAX_URI_SIZE, pipe)!=NULL){
		res.s = res_line;
		res.len=strlen(res.s);
		/* trim from right */
		while(res.len && (res.s[res.len-1]=='\r' 
				|| res.s[res.len-1]=='\n' 
				|| res.s[res.len-1]=='\t'
				|| res.s[res.len-1]==' ' )) {
			res.len--;
		}
		/* skip empty line */
		if (res.len==0) continue;
		/* ZT */
		res.s[res.len]=0;

		avp_type = 0;
		if(crt==NULL)
		{
			avp_name.n = i+1;
		} else {
			if(pv_get_avp_name(msg, &(crt->sname.pvp), &avp_name, &avp_type)!=0)
			{
				LM_ERR("can't get item name [%d]\n",i);
				goto error;
			}
		}

		avp_type |= AVP_VAL_STR;
		avp_val.s = res;
	
		if(add_avp(avp_type, avp_name, avp_val)!=0)
		{
			LM_ERR("unable to add avp\n");
			goto error;
		}
	
		if(crt)
			crt = crt->next;

		i++;
	}
	if (i==0)
		LM_DBG("no result from %s\n", cmd);
	/* success */
	ret=1;

error:
	if (ferror(pipe)) {
		LM_ERR("pipe: %d/%s\n",	errno, strerror(errno));
		ser_error=E_EXEC;
		ret=-1;
	}
	exit_status=pclose(pipe);
	if (WIFEXITED(exit_status)) { /* exited properly .... */
		/* return false if script exited with non-zero status */
		if (WEXITSTATUS(exit_status)!=0) ret=-1;
	} else { /* exited erroneously */
		LM_ERR("cmd %s failed. exit_status=%d, errno=%d: %s\n",
			cmd, exit_status, errno, strerror(errno) );
		ret=-1;
	}
	return ret;
}

