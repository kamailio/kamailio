/*
 * $Id$
 *
 * JABBER module
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../error.h"
#include "../../ut.h"
#include "../im/im_funcs.h"

#include "sip2jabber.h"

jbconnection jbc = NULL;

static int mod_init(void);
static int jab_send_message(struct sip_msg*, char*, char* );

void destroy(void);

struct module_exports exports= {
	"jabber_module",
	(char*[]){
				"jab_send_message"
			},
	(cmd_function[]){
					jab_send_message
					},
	(int[]){
				0
			},
	(fixup_function[]){
				0
		},
	1,

	NULL,   /* Module parameter names */
	NULL,   /* Module parameter types */
	NULL,   /* Module parameter variable pointers */
	0,      /* Number of module paramers */

	mod_init,   /* module initialization function */
	(response_function) 0,
	(destroy_function) destroy,
	0,
	0  /* per-child init function */
};


static int mod_init(void)
{
	DBG("JABBER: initializing\n");
	jbc = jb_init_jbconnection("gorn.fokus.gmd.de", 5222);

	if(jb_connect_to_server(jbc))
	{
		DBG("JABBER: Cannot connect to the Jabber server ...");
		return 1;
	}

	if(jb_user_auth_to_server(jbc, "tzaphy", "1qaz2wsx", "jbcl") < 0)
	{
		DBG("JABBER: Authentication to the Jabber server failed ...");
		return 1;
	}

	jb_get_roster(jbc);
	jb_send_presence(jbc, NULL, "Online", "9");
	return 0;
}


static int jab_send_message(struct sip_msg *msg, char* foo1, char * foo2)
{
	str body, user, host;
	char buff[128];

	struct to_body from;

	if ( !im_extract_body(msg,&body) )
	{
		DBG("JABBER: jab_send_message:cannot extract body from sip msg!\n");
		goto error;
	}

	if ( im_get_user(msg, &user, &host) < 0 )
	{
		DBG("JABBER: jab_send_message:cannot parse URI from sip msg!\n");
		goto error;
	}

	/*
	jb_send_msg(jbc, uri.user.s, uri.user.len + uri.host.len + 1, body.s, body.len);
	jb_send_msg(jbc, msg->first_line.u.request.uri.s+4, msg->first_line.u.request.uri.len, body.s, body.len);
	*/
	buff[0] = 0;
	strncat(buff, user.s, user.len);
	strncat(buff, "@", 1);
	strncat(buff, host.s, host.len);
	DBG("JABER:DEST: %s\n", buff);
	if(msg->from != NULL && parse_to(msg->from->body.s, msg->from->body.s + msg->from->body.len + 1, &from) >= 0)
	{
		jb_send_sig_msg(jbc, buff, user.len+host.len+1, body.s, body.len, from.body.s, from.body.len);
	}
	else
		jb_send_msg(jbc, buff, user.len+host.len+1, body.s, body.len);

	return 1;
error:
	return -1;
}


void destroy(void)
{
	jb_disconnect(jbc);
	jb_free_jbconnection(jbc);
}
