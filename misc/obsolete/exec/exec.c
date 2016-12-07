/*
 *
 * $Id$
 *
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
 * History
 * --------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-28 scratchpad removed
 * 2004-07-21 rewrite uri done via action() (bogdan)
 */


#include "../../comp_defs.h"

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
#include "../../ut.h"
#include "config.h"

int exec_msg(struct sip_msg *msg, str* cmd )
{
	FILE *pipe;
	int exit_status;
	int ret;
	char* c;

	ret=-1; /* pessimist: assume error */

	c = as_asciiz(cmd);
	if (!c) {
	    ERR("No memory left\n");
	    return -1;
	}

	pipe=popen( c, "w" );
	pkg_free(c);
	if (pipe==NULL) {
	    LOG(L_ERR, "ERROR: exec_msg: cannot open pipe: %.*s\n",
			cmd->len, ZSW(cmd->s));
		ser_error=E_EXEC;
		return -1;
	}

	if (fwrite(msg->buf, 1, msg->len, pipe)!=msg->len) {
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
		ret=-1;
	}
	exit_status=pclose(pipe);
	if (WIFEXITED(exit_status)) { /* exited properly .... */
		/* return false if script exited with non-zero status */
		if (WEXITSTATUS(exit_status)!=0) ret=-1;
	} else { /* exited erroneously */
		LOG(L_ERR, "ERROR: exec_msg: cmd %.*s failed. "
			"exit_status=%d, errno=%d: %s\n",
			cmd->len, ZSW(cmd->s), exit_status, errno, strerror(errno) );
		ret=-1;
	}
	return ret;
}

int exec_str(struct sip_msg *msg, str* cmd, char *param, int param_len) {

	struct action act;
	int cmd_len;
	FILE *pipe;
	char *cmd_line;
	int ret;
	char uri_line[MAX_URI_SIZE+1];
	int uri_cnt;
	int uri_len;
	int exit_status;
	struct run_act_ctx ra_ctx;

	/* pessimist: assume error by default */
	ret=-1;

	cmd_len=cmd->len+param_len+2;
	cmd_line=pkg_malloc(cmd_len);
	if (cmd_line==0) {
		ret=ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: exec_str: no mem for command\n");
		goto error00;
	}

	/* 'command parameter \0' */
	memcpy(cmd_line, cmd->s, cmd->len); cmd_line[cmd->len]=' ';
	memcpy(cmd_line+cmd->len+1, param, param_len);cmd_line[cmd->len+param_len+1]=0;

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
			memset(&act, 0, sizeof(act));
			act.type = SET_URI_T;
			act.val[0].type = STRING_ST;
			act.val[0].u.string = uri_line;
			init_run_actions_ctx(&ra_ctx);
			if (do_action(&ra_ctx, &act, msg)<0) {
				LOG(L_ERR,"ERROR:exec_str : SET_URI_T action failed\n");
				ser_error=E_OUT_OF_MEM;
				goto error02;
			}
		} else {
			if (ser_append_branch(msg, uri_line, uri_len, 0, 0,
									Q_UNSPECIFIED, 0)==-1) {
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
		ret=-1;
	}
	exit_status=pclose(pipe);
	if (WIFEXITED(exit_status)) { /* exited properly .... */
		/* return false if script exited with non-zero status */
		if (WEXITSTATUS(exit_status)!=0) ret=-1;
	} else { /* exited erroneously */
		LOG(L_ERR, "ERROR: exec_str: cmd %.*s failed. "
			"exit_status=%d, errno=%d: %s\n",
			cmd->len, ZSW(cmd->s), exit_status, errno, strerror(errno) );
		ret=-1;
	}
error01:
	pkg_free(cmd_line);
error00:
	return ret;
}
