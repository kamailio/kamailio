/*
 * pua_dialoginfo module - publish dialog-info from dialo module
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 * Copyright (C) 2007-2008 Dan Pascu
 * Copyright (C) 2008 Klaus Darilion IPCom
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
 * History:
 * --------
 *  2008-08-25  initial version (kd)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <time.h>

#include "../../script_cb.h"
#include "../../sr_module.h"
#include "../../parser/parse_expires.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/contact/parse_contact.h"
#include "../../str.h"
#include "../../str_list.h"
#include "../../mem/mem.h"
#include "../../pt.h"
#include "../dialog/dlg_load.h"
#include "../dialog/dlg_hash.h"
#include "../pua/pua_bind.h"
#include "pua_dialoginfo.h"

MODULE_VERSION

/* Default module parameter values */
#define DEF_INCLUDE_CALLID 1
#define DEF_INCLUDE_LOCALREMOTE 1
#define DEF_INCLUDE_TAGS 1
#define DEF_OVERRIDE_LIFETIME 0
#define DEF_CALLER_ALWAYS_CONFIRMED 0
#define DEF_INCLUDE_REQ_URI 0
#define DEF_OVERRIDE_LIFETIME 0
#define DEF_SEND_PUBLISH_FLAG -1
#define DEF_USE_PUBRURI_AVPS 0
#define DEF_PUBRURI_CALLER_AVP 0
#define DEF_PUBRURI_CALLEE_AVP 0


/* define PUA_DIALOGINFO_DEBUG to activate more verbose 
 * logging and dialog info callback debugging
 */
/* #define PUA_DIALOGINFO_DEBUG 1 */

pua_api_t pua;

struct dlg_binds dlg_api;

unsigned short pubruri_caller_avp_type;
int_str pubruri_caller_avp_name;
unsigned short pubruri_callee_avp_type;
int_str pubruri_callee_avp_name;

static str caller_dlg_var = {0, 0}; /* pubruri_caller */
static str callee_dlg_var = {0, 0}; /* pubruri_callee */

/* Module parameter variables */
int include_callid         = DEF_INCLUDE_CALLID;
int include_localremote    = DEF_INCLUDE_LOCALREMOTE;
int include_tags           = DEF_INCLUDE_TAGS;
int override_lifetime      = DEF_OVERRIDE_LIFETIME;
int caller_confirmed       = DEF_CALLER_ALWAYS_CONFIRMED;
int include_req_uri        = DEF_INCLUDE_REQ_URI;
int send_publish_flag      = DEF_SEND_PUBLISH_FLAG;
int use_pubruri_avps       = DEF_USE_PUBRURI_AVPS;
char * pubruri_caller_avp  = DEF_PUBRURI_CALLER_AVP;
char * pubruri_callee_avp  = DEF_PUBRURI_CALLEE_AVP;


send_publish_t pua_send_publish;
/** module functions */

static int mod_init(void);


static cmd_export_t cmds[]=
{
	{0, 0, 0, 0, 0, 0} 
};

static param_export_t params[]={
	{"include_callid",      INT_PARAM, &include_callid },
	{"include_localremote", INT_PARAM, &include_localremote },
	{"include_tags",        INT_PARAM, &include_tags },
	{"override_lifetime",   INT_PARAM, &override_lifetime },
	{"caller_confirmed",    INT_PARAM, &caller_confirmed },
	{"include_req_uri",     INT_PARAM, &include_req_uri },
	{"send_publish_flag",   INT_PARAM, &send_publish_flag },
	{"use_pubruri_avps",    INT_PARAM, &use_pubruri_avps },
	{"pubruri_caller_avp",  PARAM_STRING, &pubruri_caller_avp },
	{"pubruri_callee_avp",  PARAM_STRING, &pubruri_callee_avp },
	{"pubruri_caller_dlg_var",  PARAM_STR, &caller_dlg_var },
	{"pubruri_callee_dlg_var",  PARAM_STR, &callee_dlg_var },
	{0, 0, 0 }
};

struct module_exports exports= {
	"pua_dialoginfo",    /* module name */
	DEFAULT_DLFLAGS,     /* dlopen flags */
	cmds,                /* exported functions */
	params,              /* exported parameters */
	0,                   /* exported statistics */
	0,                   /* exported MI functions */
	0,                   /* exported pseudo-variables */
	0,                   /* extra processes */
	mod_init,            /* module initialization function */
	0,                   /* response handling function */
	0,                   /* destroy function */
	NULL                 /* per-child init function */
};


#ifdef PUA_DIALOGINFO_DEBUG
static void
__dialog_cbtest(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	str tag;
	struct sip_msg *msg;
	LM_ERR("dialog callback received, from=%.*s, to=%.*s\n",
			dlg->from_uri.len, dlg->from_uri.s, dlg->to_uri.len,
			dlg->to_uri.s);
	if (dlg->tag[0].len && dlg->tag[0].s ) {
		LM_ERR("dialog callback: tag[0] = %.*s",
				dlg->tag[0].len, dlg->tag[0].s);
	}
	if (dlg->tag[0].len && dlg->tag[1].s ) {
		LM_ERR("dialog callback: tag[1] = %.*s",
				dlg->tag[1].len, dlg->tag[1].s);
	}

	if (type != DLGCB_DESTROY) {
		msg = dlg_get_valid_msg(_params);
		if (!msg) {
			LM_ERR("no SIP message available in callback parameters\n");
			return;
		}

		/* get to tag*/
		if ( !msg->to) {
			// to header not defined, parse to header
			LM_ERR("to header not defined, parse to header\n");
			if (parse_headers(msg, HDR_TO_F,0)<0) {
				//parser error
				LM_ERR("parsing of to-header failed\n");
				tag.s = 0;
				tag.len = 0;
			} else if (!msg->to) {
				// to header still not defined
				LM_ERR("bad reply or missing TO header\n");
				tag.s = 0;
				tag.len = 0;
			} else 
				tag = get_to(msg)->tag_value;
		} else {
			tag = get_to(msg)->tag_value;
			if (tag.s==0 || tag.len==0) {
				LM_ERR("missing TAG param in TO hdr :-/\n");
				tag.s = 0;
				tag.len = 0;
			}
		}
		if (tag.s) {
			LM_ERR("dialog callback: msg->to->parsed->tag_value = %.*s",
					tag.len, tag.s);
		}
	}

	switch (type) {
		case DLGCB_FAILED:
			LM_ERR("dialog callback type 'DLGCB_FAILED' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_CONFIRMED_NA:
			LM_ERR("dialog callback type 'DLGCB_CONFIRMED_NA' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_CONFIRMED:
			LM_ERR("dialog callback type 'DLGCB_CONFIRMED' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_REQ_WITHIN:
			LM_ERR("dialog callback type 'DLGCB_REQ_WITHIN' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_TERMINATED:
			LM_ERR("dialog callback type 'DLGCB_TERMINATED' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_EXPIRED:
			LM_ERR("dialog callback type 'DLGCB_EXPIRED' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_EARLY:
			LM_ERR("dialog callback type 'DLGCB_EARLY' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_RESPONSE_FWDED:
			LM_ERR("dialog callback type 'DLGCB_RESPONSE_FWDED' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_RESPONSE_WITHIN:
			LM_ERR("dialog callback type 'DLGCB_RESPONSE_WITHIN' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_MI_CONTEXT:
			LM_ERR("dialog callback type 'DLGCB_MI_CONTEXT' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		case DLGCB_DESTROY:
			LM_ERR("dialog callback type 'DLGCB_DESTROY' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
			break;
		default:
			LM_ERR("dialog callback type 'unknown' received, from=%.*s\n",
					dlg->from_uri.len, dlg->from_uri.s);
	}
}
#endif

static void
__dialog_sendpublish(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	str tag = {0,0};
	str uri = {0,0};
	str target = {0,0};


	struct dlginfo_cell *dlginfo = (struct dlginfo_cell*)*_params->param;

	if(include_req_uri) {
		uri = dlginfo->req_uri;
	} else {
		uri = dlginfo->to_uri;
	}

	switch (type) {
		case DLGCB_FAILED:
		case DLGCB_TERMINATED:
		case DLGCB_EXPIRED:
			LM_DBG("dialog over, from=%.*s\n", dlginfo->from_uri.len,
					dlginfo->from_uri.s);
			dialog_publish_multi("terminated", dlginfo->pubruris_caller,
					&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
					10, 0, 0, &(dlginfo->from_contact),
					&target, send_publish_flag==-1?1:0);
			dialog_publish_multi("terminated", dlginfo->pubruris_callee,
					&uri, &(dlginfo->from_uri), &(dlginfo->callid), 0,
					10, 0, 0, &target, &(dlginfo->from_contact),
					send_publish_flag==-1?1:0);
			break;
		case DLGCB_CONFIRMED:
		case DLGCB_REQ_WITHIN:
		case DLGCB_CONFIRMED_NA:
			LM_DBG("dialog confirmed, from=%.*s\n", dlginfo->from_uri.len,
					dlginfo->from_uri.s);
			dialog_publish_multi("confirmed", dlginfo->pubruris_caller,
					&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
					dlginfo->lifetime, 0, 0, &(dlginfo->from_contact), &target,
					send_publish_flag==-1?1:0);
			dialog_publish_multi("confirmed", dlginfo->pubruris_callee, &uri,
					&(dlginfo->from_uri), &(dlginfo->callid), 0,
					dlginfo->lifetime, 0, 0, &target, &(dlginfo->from_contact),
					send_publish_flag==-1?1:0);
			break;
		case DLGCB_EARLY:
			LM_DBG("dialog is early, from=%.*s\n", dlginfo->from_uri.len,
					dlginfo->from_uri.s);
			if (include_tags) {
				/* get remotetarget */
				if ( !_params->rpl->contact && ((parse_headers(_params->rpl,
									HDR_CONTACT_F,0)<0) || !_params->rpl->contact) ) {
					LM_ERR("bad reply or missing CONTACT hdr\n");
				} else {
					if ( parse_contact(_params->rpl->contact)<0 ||
							((contact_body_t *)_params->rpl->contact->parsed)->contacts==NULL ||
							((contact_body_t *)_params->rpl->contact->parsed)->contacts->next!=NULL ) {
						LM_ERR("Malformed CONTACT hdr\n");
					} else {
						target = ((contact_body_t *)_params->rpl->contact->parsed)->contacts->uri;
					}
				}
				/* get to tag*/
				if ( !_params->rpl->to && ((parse_headers(_params->rpl, HDR_TO_F,0)<0)
							|| !_params->rpl->to) ) {
					LM_ERR("bad reply or missing TO hdr :-/\n");
					tag.s = 0;
					tag.len = 0;
				} else {
					tag = get_to(_params->rpl)->tag_value;
					if (tag.s==0 || tag.len==0) {
						LM_ERR("missing TAG param in TO hdr :-/\n");
						tag.s = 0;
						tag.len = 0;
					}
				}
				if (caller_confirmed) {
					dialog_publish_multi("confirmed", dlginfo->pubruris_caller,
							&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
							dlginfo->lifetime, &(dlginfo->from_tag), &tag,
							&(dlginfo->from_contact), &target,
							send_publish_flag==-1?1:0);
				} else {
					dialog_publish_multi("early", dlginfo->pubruris_caller,
							&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
							dlginfo->lifetime, &(dlginfo->from_tag), &tag,
							&(dlginfo->from_contact), &target,
							send_publish_flag==-1?1:0);
				}
				dialog_publish_multi("early", dlginfo->pubruris_callee, &uri,
						&(dlginfo->from_uri), &(dlginfo->callid), 0,
						dlginfo->lifetime, &tag, &(dlginfo->from_tag), &target,
						&(dlginfo->from_contact), send_publish_flag==-1?1:0);

			} else {
				if (caller_confirmed) {
					dialog_publish_multi("confirmed", dlginfo->pubruris_caller,
							&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
							dlginfo->lifetime, 0, 0, &(dlginfo->from_contact),
							&target, send_publish_flag==-1?1:0);

				} else {
					dialog_publish_multi("early", dlginfo->pubruris_caller,
							&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
							dlginfo->lifetime, 0, 0, &(dlginfo->from_contact),
							&target, send_publish_flag==-1?1:0);
				}
				dialog_publish_multi("early", dlginfo->pubruris_callee, &uri,
						&(dlginfo->from_uri), &(dlginfo->callid), 0,
						dlginfo->lifetime, 0, 0, &target,
						&(dlginfo->from_contact), send_publish_flag==-1?1:0);

			}
			break;
		default:
			LM_ERR("unhandled dialog callback type %d received, from=%.*s\n",
					type, dlginfo->from_uri.len, dlginfo->from_uri.s);
			dialog_publish_multi("terminated", dlginfo->pubruris_caller,
					&(dlginfo->from_uri), &uri, &(dlginfo->callid), 1,
					10, 0, 0, &(dlginfo->from_contact), &target,
					send_publish_flag==-1?1:0);
			dialog_publish_multi("terminated", dlginfo->pubruris_callee, &uri,
					&(dlginfo->from_uri), &(dlginfo->callid), 0,
					10, 0, 0, &target, &(dlginfo->from_contact),
					send_publish_flag==-1?1:0);

	}
}

/*
 *  Writes all avps with name avp_name to new str_list (shm mem)
 *  Be careful: returns NULL pointer if no avp present!
 *
 */
struct str_list* get_str_list(unsigned short avp_flags, int_str avp_name) {

	int_str avp_value;
	unsigned int len;
	struct str_list* list_first = 0;
	struct str_list* list_current = 0	;
	struct search_state st;

	if(!search_first_avp(avp_flags, avp_name, &avp_value, &st)) {
		return NULL;
	}

	do {

		LM_DBG("AVP found '%.*s'\n", avp_value.s.len, avp_value.s.s);

		len = sizeof(struct str_list) + avp_value.s.len;

		if(list_current) {
			list_current->next = (struct str_list*) shm_malloc( len);
			list_current=list_current->next;
		} else {
			list_current=list_first= (struct str_list*) shm_malloc( len);
		}

		if (list_current==0) {
			LM_ERR("no more shm mem (%d)\n",len);
			return 0;
		}

		memset( list_current, 0, len);

		list_current->s.s = (char*)list_current + sizeof(struct str_list);
		list_current->s.len = avp_value.s.len;
		memcpy(list_current->s.s,avp_value.s.s,avp_value.s.len);



	} while(search_next_avp(&st, &avp_value));

	return list_first;

}

struct dlginfo_cell* get_dialog_data(struct dlg_cell *dlg, int type)
{
	struct dlginfo_cell *dlginfo;
	int len;
	str* s=NULL;

	/* create dlginfo structure to store important data inside the module*/
	len = sizeof(struct dlginfo_cell)
		+ dlg->from_uri.len
		+ dlg->to_uri.len
		+ dlg->callid.len
		+ dlg->tag[0].len
		+ dlg->req_uri.len
		+ dlg->contact[0].len;

	dlginfo = (struct dlginfo_cell*)shm_malloc( len );
	if (dlginfo==0) {
		LM_ERR("no more shm mem (%d)\n",len);
		return NULL;
	}
	memset( dlginfo, 0, len);

	/* copy from dlg structure to dlginfo structure */
	dlginfo->lifetime     = override_lifetime ? override_lifetime : dlg->lifetime;
	dlginfo->from_uri.s   = (char*)dlginfo + sizeof(struct dlginfo_cell);
	dlginfo->from_uri.len = dlg->from_uri.len;
	dlginfo->to_uri.s     = dlginfo->from_uri.s + dlg->from_uri.len;
	dlginfo->to_uri.len   = dlg->to_uri.len;
	dlginfo->callid.s     = dlginfo->to_uri.s + dlg->to_uri.len;
	dlginfo->callid.len   = dlg->callid.len;
	dlginfo->from_tag.s   = dlginfo->callid.s + dlg->callid.len;
	dlginfo->from_tag.len = dlg->tag[0].len;
	dlginfo->req_uri.s    = dlginfo->from_tag.s + dlginfo->from_tag.len;
	dlginfo->req_uri.len  = dlg->req_uri.len;
	dlginfo->from_contact.s   = dlginfo->req_uri.s + dlginfo->req_uri.len;
	dlginfo->from_contact.len = dlg->contact[0].len;

	memcpy(dlginfo->from_uri.s, dlg->from_uri.s, dlg->from_uri.len);
	memcpy(dlginfo->to_uri.s, dlg->to_uri.s, dlg->to_uri.len);
	memcpy(dlginfo->callid.s, dlg->callid.s, dlg->callid.len);
	memcpy(dlginfo->from_tag.s, dlg->tag[0].s, dlg->tag[0].len);
	memcpy(dlginfo->req_uri.s, dlg->req_uri.s, dlg->req_uri.len);
	memcpy(dlginfo->from_contact.s, dlg->contact[0].s, dlg->contact[0].len);

	if (use_pubruri_avps) {
		if(type==DLGCB_CREATED) {
			dlginfo->pubruris_caller = get_str_list(pubruri_caller_avp_type,
					pubruri_caller_avp_name);
			dlginfo->pubruris_callee = get_str_list(pubruri_callee_avp_type,
					pubruri_callee_avp_name);

			if(dlginfo->pubruris_callee!=NULL && callee_dlg_var.len>0)
				dlg_api.set_dlg_var(dlg, &callee_dlg_var,
						&dlginfo->pubruris_callee->s);

			if(dlginfo->pubruris_caller!=NULL && caller_dlg_var.len>0)
				dlg_api.set_dlg_var(dlg, &caller_dlg_var,
						&dlginfo->pubruris_caller->s);

		} else {
			if(caller_dlg_var.len>0
					&& (s = dlg_api.get_dlg_var(dlg, &caller_dlg_var))!=0) {
				dlginfo->pubruris_caller =
					(struct str_list*)shm_malloc( sizeof(struct str_list) );
				if (dlginfo->pubruris_caller==0) {
					LM_ERR("no more shm mem (%d)\n", (int) sizeof(struct str_list));
					free_dlginfo_cell(dlginfo);
					return NULL;
				}
				memset( dlginfo->pubruris_caller, 0, sizeof(struct str_list));
				dlginfo->pubruris_caller->s=*s;
				LM_DBG("Found pubruris_caller in dialog '%.*s'\n",
						dlginfo->pubruris_caller->s.len, dlginfo->pubruris_caller->s.s);
			}

			if(callee_dlg_var.len>0
					&& (s = dlg_api.get_dlg_var(dlg, &callee_dlg_var))!=0) {
				dlginfo->pubruris_callee =
					(struct str_list*)shm_malloc( sizeof(struct str_list) );
				if (dlginfo->pubruris_callee==0) {
					LM_ERR("no more shm mem (%d)\n", (int) sizeof(struct str_list));
					free_dlginfo_cell(dlginfo);
					return NULL;
				}
				memset( dlginfo->pubruris_callee, 0, sizeof(struct str_list));
				dlginfo->pubruris_callee->s=*s;
				LM_DBG("Found pubruris_callee in dialog '%.*s'\n",
						dlginfo->pubruris_callee->s.len, dlginfo->pubruris_callee->s.s);
			}
		}

		if(dlginfo->pubruris_caller == 0 && dlginfo->pubruris_callee == 0 ) {
			/* No reason to save dlginfo, we have nobody to publish to */
			LM_DBG("Neither pubruris_caller nor pubruris_callee found.\n");
			free_dlginfo_cell(dlginfo);
			return NULL;
		}
	} else {
		dlginfo->pubruris_caller =
			(struct str_list*)shm_malloc( sizeof(struct str_list) );
		if (dlginfo->pubruris_caller==0) {
			LM_ERR("no more shm mem (%d)\n", (int) sizeof(struct str_list));
			free_dlginfo_cell(dlginfo);
			return NULL;
		}
		memset( dlginfo->pubruris_caller, 0, sizeof(struct str_list));
		dlginfo->pubruris_caller->s=dlginfo->from_uri;

		dlginfo->pubruris_callee =
			(struct str_list*)shm_malloc( sizeof(struct str_list) );
		if (dlginfo->pubruris_callee==0) {
			LM_ERR("no more shm mem (%d)\n", (int) sizeof(struct str_list));
			free_dlginfo_cell(dlginfo);
			return NULL;
		}
		memset( dlginfo->pubruris_callee, 0, sizeof(struct str_list));

		if(include_req_uri) {
			dlginfo->pubruris_callee->s = dlginfo->req_uri;
		} else {
			dlginfo->pubruris_callee->s = dlginfo->to_uri;
		}
	}

	/* register dialog callbacks which triggers sending PUBLISH */
	if (dlg_api.register_dlgcb(dlg,
				DLGCB_FAILED| DLGCB_CONFIRMED_NA | DLGCB_TERMINATED
				| DLGCB_EXPIRED | DLGCB_REQ_WITHIN | DLGCB_EARLY,
				__dialog_sendpublish, dlginfo, free_dlginfo_cell) != 0) {
		LM_ERR("cannot register callback for interesting dialog types\n");
		free_dlginfo_cell(dlginfo);
		return NULL;
	}

#ifdef PUA_DIALOGINFO_DEBUG
	/* dialog callback testing (registered last to be executed frist) */
	if (dlg_api.register_dlgcb(dlg, 
				DLGCB_FAILED| DLGCB_CONFIRMED_NA | DLGCB_CONFIRMED
				| DLGCB_REQ_WITHIN | DLGCB_TERMINATED | DLGCB_EXPIRED
				| DLGCB_EARLY | DLGCB_RESPONSE_FWDED | DLGCB_RESPONSE_WITHIN
				| DLGCB_MI_CONTEXT | DLGCB_DESTROY,
				__dialog_cbtest, NULL, NULL) != 0) {
		LM_ERR("cannot register callback for all dialog types\n");
		free_dlginfo_cell(dlginfo);
		return NULL;
	}
#endif

	return(dlginfo);
}

	static void
__dialog_created(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct sip_msg *request = _params->req;
	struct dlginfo_cell *dlginfo;

	if (request->REQ_METHOD != METHOD_INVITE)
		return;

	if(send_publish_flag > -1 && !(request->flags & (1<<send_publish_flag)))
		return;

	LM_DBG("new INVITE dialog created: from=%.*s\n",
			dlg->from_uri.len, dlg->from_uri.s);

	dlginfo=get_dialog_data(dlg, type);
	if(dlginfo==NULL)
		return;

	dialog_publish_multi("Trying", dlginfo->pubruris_caller,
			&(dlg->from_uri),
			(include_req_uri)?&(dlg->req_uri):&(dlg->to_uri),
			&(dlg->callid), 1, dlginfo->lifetime,
			0, 0, 0, 0, (send_publish_flag==-1)?1:0);

}

	static void
__dialog_loaded(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params)
{
	struct dlginfo_cell *dlginfo;

	LM_DBG("INVITE dialog loaded: from=%.*s\n", dlg->from_uri.len, dlg->from_uri.s);

	dlginfo=get_dialog_data(dlg, type);
	if(dlginfo!=NULL) free_dlginfo_cell(dlginfo);
}


/**
 * init module function
 */
static int mod_init(void)
{
	bind_pua_t bind_pua;

	str s;
	pv_spec_t avp_spec;

	if(caller_dlg_var.len<=0)
		LM_WARN("pubruri_caller_dlg_var is not set - restore on restart disabled\n");

	if(callee_dlg_var.len<=0)
		LM_WARN("pubruri_callee_dlg_var is not set - restore on restart disabled\n");

	bind_pua= (bind_pua_t)find_export("bind_pua", 1,0);
	if (!bind_pua)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}

	if (bind_pua(&pua) < 0)
	{
		LM_ERR("Can't bind pua\n");
		return -1;
	}
	if(pua.send_publish == NULL)
	{
		LM_ERR("Could not import send_publish\n");
		return -1;
	}
	pua_send_publish= pua.send_publish;

	/* bind to the dialog API */
	if (load_dlg_api(&dlg_api)!=0) {
		LM_ERR("failed to find dialog API - is dialog module loaded?\n");
		return -1;
	}
	/* register dialog creation callback */
	if (dlg_api.register_dlgcb(NULL, DLGCB_CREATED, __dialog_created, NULL, NULL) != 0) {
		LM_ERR("cannot register callback for dialog creation\n");
		return -1;
	}
	/* register dialog loaded callback */
	if (dlg_api.register_dlgcb(NULL, DLGCB_LOADED, __dialog_loaded, NULL, NULL) != 0) {
		LM_ERR("cannot register callback for dialog loaded\n");
		return -1;
	}

	if(use_pubruri_avps) {

		if(!(pubruri_caller_avp && *pubruri_caller_avp)
				&& (pubruri_callee_avp && *pubruri_callee_avp)) {
			LM_ERR("pubruri_caller_avp and pubruri_callee_avp must be set,"
					" if use_pubruri_avps is enabled\n");
			return -1;
		}

		s.s = pubruri_caller_avp; s.len = strlen(s.s);
		if (pv_parse_spec(&s, &avp_spec)==0	|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", pubruri_caller_avp);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &pubruri_caller_avp_name,
					&pubruri_caller_avp_type)!=0) {
			LM_ERR("[%s]- invalid AVP definition\n", pubruri_caller_avp);
			return -1;
		}

		s.s = pubruri_callee_avp; s.len = strlen(s.s);
		if (pv_parse_spec(&s, &avp_spec)==0	|| avp_spec.type!=PVT_AVP) {
			LM_ERR("malformed or non AVP %s AVP definition\n", pubruri_callee_avp);
			return -1;
		}
		if(pv_get_avp_name(0, &avp_spec.pvp, &pubruri_callee_avp_name,
					&pubruri_callee_avp_type)!=0) {
			LM_ERR("[%s]- invalid AVP definition\n", pubruri_callee_avp);
			return -1;
		}

	}

	return 0;
}

void free_dlginfo_cell(void *param) {

	struct dlginfo_cell *cell = NULL;

	if(param==NULL)
		return;

	cell = param;
	free_str_list_all(cell->pubruris_caller);
	free_str_list_all(cell->pubruris_callee);

	/*if (cell->to_tag) {
		shm_free(cell->to_tag);
	}*/
	shm_free(param);
}


void free_str_list_all(struct str_list * del_current) {

	struct str_list* del_next;

	while(del_current) {

		del_next = del_current->next;
		shm_free(del_current);

		del_current=del_next;
	}

}
