/*
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2004-06-02: applied patch from Maxim, rewriteuri and rewriteuser merged,
 *             branching support added, "check_new_uri","max_branches" params
 *             added (bogdan)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../action.h"
#include "../../sr_module.h"
#include "../../error.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../globals.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "my_exec.h"
#include "config.h"

MODULE_VERSION

#define MAX_BUFFER_LEN  1024
#define EXT_REWRITE_URI    1
#define EXT_REWRITE_USER   2

static int ext_child_init(int);
static int ext_rewriteuser(struct sip_msg*, char*, char* );
static int ext_rewriteuri(struct sip_msg*, char*, char* );
static int fixup_ext_rewrite(void** param, int param_no);

static int check_new_uri = 1;
static int max_branches = 0;
static qvalue_t def_qv  = 1000;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"ext_rewriteuser", ext_rewriteuser, 1, fixup_ext_rewrite, REQUEST_ROUTE},
	{"ext_rewriteuri",  ext_rewriteuri,  1, fixup_ext_rewrite, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"check_new_uri", INT_PARAM, &check_new_uri},
	{"max_branches",  INT_PARAM, &max_branches},
	{0, 0, 0}
};


struct module_exports exports = {
	"ext",
	cmds,   /* Exported functions */
	params, /* Exported parameters */
	0,      /* module initialization function */
	0,      /* response function */
	0,      /* module exit function */
	0,
	(child_init_function) ext_child_init  /* per-child init function */
};




static int ext_child_init(int child)
{
	return init_ext(child);
}



static int fixup_ext_rewrite(void** param, int param_no)
{
	if (param_no==1) {
		if (access(*param,X_OK)<0) {
			LOG(L_WARN, "WARNING: fixup_ext_rewrite: program '%s'"
			"not executable : %s (shell command?)\n",
				(char *) *param, strerror(errno));
		}
		if (access(SHELL,X_OK)<0) {
			LOG(L_ERR, "ERROR: fixup_ext_rewrite: %s : %s\n",
				SHELL, strerror(errno));
			return E_UNSPEC;
		}
	}
	return 0;
}



static  char *run_ext_prog(char *cmd, char *in, int in_len, int *out_len)
{
	static char buf[MAX_PIPE_BUFFER_LEN];
	int len;
	int ret;

	/* launch the external program */
	if ( start_prog(cmd)!=0 ) {
		ser_error=E_EXEC;
		LOG(L_ERR,"ERROR:run_ext_prog: cannot launch external program\n");
		return 0;
	}

	DBG("DEBUG:run_ext_prog: sending <%.*s>\n",in_len,in);
	/* feeding the program with the uri */
	if ( sendto_prog(in,in_len,1)!=in_len ) {
		LOG(L_ERR,"ERROR:run_ext_prog: cannot send input to the external "
			"program -> kill it\n");
		goto kill_it;
	}
	close_prog_input();

	/* gets its response */
	len = 0;
	do {
		ret = recvfrom_prog(buf-len,1024-len);
		if (ret==-1 && errno!=EINTR) {
			LOG(L_ERR,"ERROR:run_ext_prog: cannot read from the external "
				"program (%s) -> kill it\n",strerror(errno));
			goto kill_it;
		}
		len += ret;
	}while(ret);

	close_prog_output();
	ret = is_finished();
	DBG("DEBUG:run_ext_prog: recv <%.*s> [%d] ; status=%d\n",
		len, buf,len,is_finished());

	if (ret!=0) {
		*out_len = 0;
		return 0;
	}

	*out_len = len;
	return buf;
kill_it:
	ser_error=E_EXEC;
	kill_prog();
	wait_prog();
	close_prog_input();
	close_prog_output();
	*out_len = 0;
	return 0;
}



static int complete_uri( str *user ,str *uri, struct sip_uri* puri )
{
	static char uri_s[MAX_URI_SIZE];
	int uri_len;
	char *p;

	/* compute the len and check for overflow */
	uri_len = uri->len + user->len - puri->user.len;
	if (uri_len>=MAX_URI_SIZE-1) {
		LOG(L_ERR,"ERROR:ext:complete_uri: new URI will be to long %d\n ",
			uri_len);
		return -1;
	}

	p = uri_s;
	/* copy the part before username */
	memcpy( p, uri->s, puri->user.s-uri->s);
	p += puri->user.s-uri->s;
	/* copy the username */
	memcpy( p, user->s, user->len);
	p += user->len;
	/* copy the part after username */
	memcpy( p, puri->user.s+puri->user.len,
		(uri->s+uri->len)-(puri->user.s+puri->user.len));
	p += (uri->s+uri->len)-(puri->user.s+puri->user.len);
	*p = 0;

	if (p-uri_s!=uri_len) {
		LOG(L_ERR,"ERROR:ext:complete_uri: len missedmatched computed[%d] "
			"written[%d]\n",uri_len,p-uri_s);
		return -1;
	}

	user->s = uri_s;
	user->len = uri_len;
	return 0;
}



static inline int is_space(char c)
{
	return ( c==' ' || c=='\t' || c=='\n' || c=='\r');
}


static int ext_rewrite(struct sip_msg *msg, char *cmd, int type )
{
	struct sip_uri parsed_uri;
	struct action act;
	str  buf;
	str  new_val;
	str  *uri;
	char *buf_end;
	char *c;
	int  i;

	/* take the uri out -> from new_uri or RURI */
	if (msg->new_uri.s && msg->new_uri.len)
		uri = &(msg->new_uri);
	else if (msg->first_line.u.request.uri.s &&
	msg->first_line.u.request.uri.len )
		uri = &(msg->first_line.u.request.uri);
	else {
		LOG(L_ERR,"ERROR:ext_rewrite: cannot find Ruri in msg!\n");
		goto error;
	}

	/*  run the external program */
	if (type==EXT_REWRITE_URI) {
		buf.s = run_ext_prog(cmd, uri->s, uri->len, &buf.len );
	} else {
		/* parse it to identify username */
		if (parse_uri(uri->s,uri->len,&parsed_uri)<0 ) {
			LOG(L_ERR,"ERROR:ext_rewrite : cannot parse Ruri!\n");
			return -1;
		}
		/* drop it if no username found */
		if (!parsed_uri.user.s && !parsed_uri.user.len) {
			LOG(L_INFO,"INFO:ext_rewrite: username not present in RURI->"
				" exiting without error\n");
			goto done;
		}
		/*  run the external program */
		buf.s = run_ext_prog(cmd, parsed_uri.user.s, parsed_uri.user.len,
			&buf.len );
	}

	/* strip initial space chars and see if it's empty */
	while ( buf.s && is_space(*(buf.s)) ) {
		buf.s++;
		buf.len--;
	}
	if ( !buf.s || !buf.len) {
		LOG(L_ERR,"ERROR:ext_rewrite: run_ext_prog returned null, "
			"ser_error=%d\n", ser_error );
		goto error;
	}

	/* process the ext prog output */
	i = 0;
	c = buf.s;
	buf_end = buf.s + buf.len;

	do {
		new_val.s   = c;
		/* go to the end of value */
		while ( c<buf_end && !is_space(*c) )
			c++;
		new_val.len = c - new_val.s;
		/* is the uri empty? */
		if (!new_val.len) {
			LOG(L_ERR,"ERROR:ext_rewrite:error parsing external prog output"
			": <%.*s> at char[%c]\n",buf.len,buf.s,*c);
			goto error;
		}
		new_val.s[new_val.len] = '\0';
		/* jump over space, tab, \n and \r */
		c++;
		while ( c<buf_end && is_space(*c) )
			c++;

		/* check the uri if it's correct */
		if (type==EXT_REWRITE_URI && check_new_uri &&
		parse_uri(new_val.s,new_val.len,&parsed_uri)<0) {
			LOG(L_ERR,"ERROR:ext_rewrite: ext prog returned invalid uri "
				"<%.*s>!\n", new_val.len, new_val.s);
			goto error;
		}

		/* now, use it! */
		DBG("DEBUG:ext_rewrite: setting <%.*s> [%d] on branch %d\n",
			new_val.len, new_val.s,new_val.len, i);

		if (i==0) {
			/* first returned uri */
			memset(&act, 0, sizeof(act));
			act.type = (type==EXT_REWRITE_URI)?SET_URI_T:SET_USER_T;
			act.p1_type = STRING_ST;
			act.p1.string = new_val.s;
			if (do_action(&act, msg)<0) {
				LOG(L_ERR,"ERROR:ext_rewrite : SET_XXXX_T action failed\n");
				goto error;
			}
		} else {
			/* append branches */
			if (type==EXT_REWRITE_USER) {
				if (complete_uri( &new_val , uri, &parsed_uri )!=0 )
					goto error;
			}
			if (append_branch( msg, new_val.s, new_val.len, def_qv)==-1) {
				LOG(L_ERR,"ERROR:ext_rewrite : append_branch failed\n");
				goto error;
			}
		}

		/* are we still allow to add new uris ? */
		if (c<buf_end && i>=max_branches) {
			LOG(L_NOTICE,"LOG:ext_rewrite: discarding remaining output "
				"<%.*s>\n", buf.len-(c-buf.s),c);
			break;
		}
		/* goto next uri */
		i++;
	}while(c<buf_end);

done:
	return 1;
error:
	return -1;
}



static int ext_rewriteuri(struct sip_msg *msg, char *cmd, char *foo_str )
{
	return  ext_rewrite( msg, cmd, EXT_REWRITE_URI);
}



static int ext_rewriteuser(struct sip_msg *msg, char *cmd, char *foo_str )
{
	return  ext_rewrite( msg, cmd, EXT_REWRITE_USER);
}

