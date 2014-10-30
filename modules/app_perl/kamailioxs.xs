/*
 * $Id$
 *
 * Perl module for Kamailio
 *
 * Copyright (C) 2006 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
 *
 * This file is part of kamailio, a free SIP server.
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

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include <unistd.h>
#undef load_module

/* perl.h defines union semun */
#ifdef USE_SYSV_SEM
# undef _SEM_SEMUN_UNDEFINED
#endif

#include "../../sr_module.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../usr_avp.h"
#include "../../action.h"
#include "../../flags.h"
#include "../../pvar.h"
#include "../../dset.h"
#include "../../mem/mem.h"
#include "../../route_struct.h"
#include "../../qvalue.h"
#include "../../dprint.h"

extern int unsafemodfnc;

enum xs_uri_members {
	XS_URI_USER = 0,
	XS_URI_PASSWD,
	XS_URI_HOST,
	XS_URI_PORT,
	XS_URI_PARAMS,
	XS_URI_HEADERS,
	XS_URI_TRANSPORT,
	XS_URI_TTL,
	XS_URI_USER_PARAM,
	XS_URI_MADDR,
	XS_URI_METHOD,
	XS_URI_LR,
	XS_URI_R2,
	XS_URI_TRANSPORT_VAL,
	XS_URI_TTL_VAL,
	XS_URI_USER_PARAM_VAL,
	XS_URI_MADDR_VAL,
	XS_URI_METHOD_VAL,
	XS_URI_LR_VAL,
	XS_URI_R2_VAL
	
	/* These members are no strings:
		unsigned short port_no;
	unsigned short proto; / * from transport * /
	uri_type type; / * uri scheme */
};

/*
 * Return the sip_msg struct referred to by perl reference sv
 */
struct sip_msg * sv2msg(SV *sv) {
	struct sip_msg* m;
	if (SvROK(sv)) {
		sv = SvRV(sv);
		if (SvIOK(sv)) {
			m = INT2PTR(struct sip_msg*, SvIV(sv));
			return m;
		}
	}
	return NULL; /* In case of error above... */
}

struct sip_uri * sv2uri(SV *sv) {
	struct sip_uri* u;
	if (SvROK(sv)) {
		sv = SvRV(sv);
		if (SvIOK(sv)) {
			u = INT2PTR(struct sip_uri*, SvIV(sv));
			return u;
		}
	}
	return NULL; /* In case of error above... */
}

struct action * sv2action(SV *sv) {
	struct action* a;
	if (SvROK(sv)) {
		sv = SvRV(sv);
		if (SvIOK(sv)) {
			a = INT2PTR(struct action*, SvIV(sv));
			return a;
		}
	}
	return NULL; /* In case of error above... */
}

/*
 * We have a private function for two reasons:
 * a) Return SIP_INVALID even if type was sth different
 * b) easy access
 */

inline static int getType(struct sip_msg *msg) {
	int t = SIP_INVALID;

	if (!msg) return SIP_INVALID;

	switch ((msg->first_line).type) {
		case SIP_REQUEST:	t = SIP_REQUEST; break;
		case SIP_REPLY:		t = SIP_REPLY; break;
	}
	return t;
}
		

SV *getStringFromURI(SV *self, enum xs_uri_members what) {
	struct sip_uri *myuri = sv2uri(self);
	str *ret = NULL;

	if (!myuri) {
		LM_ERR("Invalid URI reference\n");
		ret = NULL;
	} else {
		
		switch (what) {
			case XS_URI_USER:	ret = &(myuri->user);
						break;
			case XS_URI_HOST:	ret = &(myuri->host);
						break;
			case XS_URI_PASSWD:	ret = &(myuri->passwd);
						break;
			case XS_URI_PORT:	ret = &(myuri->port);
						break;
			case XS_URI_PARAMS:	ret = &(myuri->params);
						break;
			case XS_URI_HEADERS:	ret = &(myuri->headers);
						break;
			case XS_URI_TRANSPORT:	ret = &(myuri->transport);
						break;
			case XS_URI_TTL:		ret = &(myuri->ttl);
						break;
			case XS_URI_USER_PARAM:	ret = &(myuri->user_param);
						break;
			case XS_URI_MADDR:	ret = &(myuri->maddr);
						break;
			case XS_URI_METHOD:	ret = &(myuri->method);
						break;
			case XS_URI_LR:		ret = &(myuri->lr);
						break;
			case XS_URI_R2:		ret = &(myuri->r2);
						break;
			case XS_URI_TRANSPORT_VAL:	ret = &(myuri->transport_val);
						break;
			case XS_URI_TTL_VAL:	ret = &(myuri->ttl_val);
						break;
			case XS_URI_USER_PARAM_VAL:	ret = &(myuri->user_param_val);
						break;
			case XS_URI_MADDR_VAL:	ret = &(myuri->maddr_val);
						break;
			case XS_URI_METHOD_VAL:	ret = &(myuri->method_val);
						break;
			case XS_URI_LR_VAL:	ret = &(myuri->lr_val);
						break;
			case XS_URI_R2_VAL:	ret = &(myuri->r2_val);
						break;

			default:	LM_INFO("Unknown URI element"
						" requested: %d\n", what);
					break;
		}
	}

	if ((ret) && (ret->len)) {
		return sv_2mortal(newSVpv(ret->s, ret->len));
	} else {
		return &PL_sv_undef;
	}
}



/*
 * Calls an exported function. Parameters are copied and fixup'd.
 *
 * Return codes:
 *   -1 - Function not available (or other error).
 *    1 - Function was called. Its return value is returned via the retval
 *        parameter.
 */

int moduleFunc(struct sip_msg *m, char *func,
	       char *param1, char *param2,
	       int *retval) {

	sr31_cmd_export_t* exp_func_struct;
	struct action *act;
	unsigned mod_ver;
	char *argv[2];
	int argc = 0;
	struct run_act_ctx ra_ctx;

	if (!func) {
		LM_ERR("moduleFunc called with null function name. Error.");
		return -1;
	}

	if ((!param1) && param2) {
		LM_ERR("moduleFunc called with parameter 1 UNSET and"
			   " parameter 2 SET. Error.");
		return -1;
	}


	if (param1) {
		argv[0] = (char *)pkg_malloc(strlen(param1)+1);
		if (!argv[0]) {
			LM_ERR("not enough pkg mem\n");
			return -1;
		}
		strcpy(argv[0], param1);
		argc++;
	} else {
		argv[0] = NULL;
	}

	if (param2) {
		argv[1] = (char *)pkg_malloc(strlen(param2)+1);
		if (!argv[1]) {
			LM_ERR("not enough pkg mem\n");
			if (argv[0]) pkg_free(argv[0]);
			return -1;
		}
		strcpy(argv[1], param2);
		argc++;
	} else {
		argv[1] = NULL;
	}

	exp_func_struct = find_export_record(func, argc, 0, &mod_ver);
	if (!exp_func_struct || mod_ver < 1) {
		LM_ERR("function '%s' called, but not available.", func);
		*retval = -1;
		if (argv[0]) pkg_free(argv[0]);
		if (argv[1]) pkg_free(argv[1]);
		return -1;
	}

	act = mk_action(MODULE2_T, 4 /* number of (type, value) pairs */,
					MODEXP_ST, exp_func_struct, /* function */
					NUMBER_ST, 2,  /* parameter number */
					STRING_ST, argv[0], /* param. 1 */
					STRING_ST, argv[1]  /* param. 2 */
			);


	if (!act) {
		LM_ERR("action structure could not be created. Error.");
		if (argv[0]) pkg_free(argv[0]);
		if (argv[1]) pkg_free(argv[1]);
		return -1;
	}


	if (exp_func_struct->fixup) {
		if (!unsafemodfnc) {
			LM_ERR("Module function '%s' is unsafe. Call is refused.\n", func);
			if (argv[0]) pkg_free(argv[0]);
			if (argv[1]) pkg_free(argv[1]);
			*retval = -1;
			return -1;
		}

		if (argc>=2) {
			*retval = exp_func_struct->fixup(&(act->val[3].u.data), 2);
			if (*retval < 0) {
				LM_ERR("Error in fixup (2)\n");
				return -1;
			}
			act->val[3].type = MODFIXUP_ST;
		}
		if (argc>=1) {
			*retval = exp_func_struct->fixup(&(act->val[2].u.data), 1);
			if (*retval < 0) {
				LM_ERR("Error in fixup (1)\n");
				return -1;
			}
			act->val[2].type = MODFIXUP_ST;
		}
		if (argc==0) {
			*retval = exp_func_struct->fixup(0, 0);
			if (*retval < 0) {
				LM_ERR("Error in fixup (0)\n");
				return -1;
			}
		}
	}

	init_run_actions_ctx(&ra_ctx);
	*retval = do_action(&ra_ctx, act, m);

	if ((act->val[3].type == MODFIXUP_ST) && (act->val[3].u.data)) {
		/* pkg_free(act->elem[3].u.data); */
		LM_WARN("moduleFunction: A fixup function was called. "
				"This currently creates a memory leak.\n");
	}

	if ((act->val[2].type == MODFIXUP_ST) && (act->val[2].u.data)) {
		/* pkg_free(act->elem[2].u.data); */
		LM_WARN("moduleFunction: A fixup function was called. "
				"This currently creates a memory leak.\n");
	}

	if (argv[0]) pkg_free(argv[0]);
	if (argv[1]) pkg_free(argv[1]);

	pkg_free(act);
	
	return 1;
}


/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
	struct action act;
	struct run_act_ctx ra_ctx;

	act.type = SET_URI_T;
	act.val[0].type = STRING_ST;
	act.val[0].u.string = _s;
	act.next = 0;

	init_run_actions_ctx(&ra_ctx);	
	if (do_action(&ra_ctx, &act, _m) < 0)
	{
		LM_ERR("rewrite_ruri: Error in do_action\n");
		return -1;
	}
	return 0;
}


/**
 * Compile a string with pseudo variables substituted by their values.
 * A string buffer is allocated. Deallocate afterwards!
 */
char *pv_sprintf(struct sip_msg *m, char *fmt) {
	int buf_size = 4096;
	static char out[4096];
	pv_elem_t *model;
	str s;
	char *ret;

	s.s = fmt; s.len = strlen(s.s);
	if(pv_parse_format(&s, &model) < 0) {
		LM_ERR("pv_sprintf: wrong format[%s]!\n",
			fmt);
		return NULL;
	}

	if(pv_printf(m, model, out, &buf_size) < 0) {
		LM_ERR("pv_printf: failed to print pv value\n");
		ret = NULL;
	} else {
		ret = strdup(out);
	}
	pv_elem_free_all(model);

	return ret;
}

/**
 * Convert an SV to an int_str struct. Needed in AVP package.
 * - val: SV to convert.
 * - is: pointer to resulting int_str
 * - flags: pointer to flags to set
 * - strflag: flag mask to be or-applied for string match
 */

inline int sv2int_str(SV *val, int_str *is,
		      unsigned short *flags, unsigned short strflag) {
	char *s;
	STRLEN len;

	if (!SvOK(val)) {
		LM_ERR("AVP:sv2int_str: Invalid value "
			"(not a scalar).\n");
		return 0;
	}
	
	if (SvIOK(val)) { /* numerical name */
		is->n = SvIV(val);
		return 1;
	} else if (SvPOK(val)) {
		s = SvPV(val, len);
		is->s.len = len;
		is->s.s = s;
		(*flags) |= strflag;
		return 1;
	} else {
		LM_ERR("AVP:sv2int_str: Invalid value "
			"(neither string nor integer).\n");
		return 0;
	}
}

/* ************************************************************************ */
/* Object methods begin here */

=head1 Kamailio

This module provides access to a limited number of Kamailio core functions.
As the most interesting functions deal with SIP messages, they are located
in the Kamailio::Message class below.

=cut

MODULE = Kamailio PACKAGE = Kamailio

=head2 log(level,message)

Logs the message with Kamailio's logging facility. The logging level
is one of the following:

 * L_ALERT
 * L_CRIT
 * L_ERR
 * L_WARN
 * L_NOTICE
 * L_INFO
 * L_DBG

Please note that this method is I<NOT> automatically exported, as it collides
with the perl function log (which calculates the logarithm). Either explicitly
import the function (via C<use Kamailio qw ( log );>), or call it with its full
name:

 Kamailio::log(L_INFO, "foobar");

=cut

void
log(level, log)
    int level
    char *log
  PREINIT:
  INIT:
  CODE:
	switch (level) {
	case L_ALERT:	LM_ALERT("%s", log); break;
	case L_CRIT:	LM_CRIT("%s", log); break;
	case L_ERR:	LM_ERR("%s", log); break;
	case L_WARN:	LM_WARN("%s", log); break;
	case L_NOTICE:	LM_NOTICE("%s", log); break;
	case L_INFO:	LM_INFO("%s", log); break;
	default:	LM_DBG("%s", log); break;
	}
  OUTPUT:



MODULE = Kamailio PACKAGE = Kamailio::Message

PROTOTYPES: ENABLE

=head1 Kamailio::Message

This package provides access functions for an Kamailio C<sip_msg> structure and
its sub-components. Through its means it is possible to fully configure
alternative routing decisions.

=cut

=head2 getType()

Returns one of the constants SIP_REQUEST, SIP_REPLY, SIP_INVALID stating the
type of the current message.

=cut

int
getType(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
  	RETVAL = getType(msg);
  OUTPUT:
  	RETVAL
	
	

=head2 getStatus()

Returns the status code of the current Reply message. This function is invalid
in Request context!

=cut

SV *
getStatus(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REPLY) {
			LM_ERR("getStatus: Status not available in"
				" non-reply messages.");
			ST(0) = &PL_sv_undef;
		} else {
			ret = &((msg->first_line).u.reply.status);
			ST(0) = sv_2mortal(newSVpv(ret->s, ret->len));
		}
	}


=head2 getReason()

Returns the reason of the current Reply message. This function is invalid
in Request context!

=cut

SV *
getReason(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REPLY) {
			LM_ERR("getReason: Reason not available in"
				" non-reply messages.");
			ST(0) = &PL_sv_undef;
		} else {
			ret = &((msg->first_line).u.reply.reason);
			ST(0) = sv_2mortal(newSVpv(ret->s, ret->len));
		}
	}


=head2 getVersion()

Returns the version string of the current SIP message.

=cut

SV *
getVersion(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) == SIP_REQUEST) {
			ret = &((msg->first_line).u.request.version);
		} else { /* SIP_REPLY */
			ret = &((msg->first_line).u.reply.version);
		}
		ST(0) = sv_2mortal(newSVpv(ret->s, ret->len));
	}


=head2 getRURI()

This function returns the recipient URI of the present SIP message:

C<< my $ruri = $m->getRURI(); >>

getRURI returns a string. See L</"getParsedRURI()"> below how to receive a
parsed structure.

This function is valid in request messages only.

=cut

SV *
getRURI(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LM_ERR("Not a request message - "
				"no RURI available.\n");
			ST(0) = &PL_sv_undef;
		} else {
			ret = &((msg->first_line).u.request.uri);
			ST(0) = sv_2mortal(newSVpv(ret->s, ret->len));
		}
	}


=head2 getMethod()

Returns the current method, such as C<INVITE>, C<REGISTER>, C<ACK> and so on.

C<< my $method = $m->getMethod(); >>

This function is valid in request messages only.

=cut

char *
getMethod(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LM_ERR("Not a request message - "
				"no method available.\n");
			ST(0) = &PL_sv_undef;
		} else {
			ret = &((msg->first_line).u.request.method);
			ST(0) = sv_2mortal(newSVpv(ret->s, ret->len));
		}
	}


=head2 getFullHeader()

Returns the full message header as present in the current message.
You might use this header to further work with it with your
favorite MIME package.

C<< my $hdr = $m->getFullHeader(); >>

=cut

SV *
getFullHeader(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    char *firsttoken;
    long headerlen;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) == SIP_INVALID) {
			LM_ERR("getFullHeader: Invalid message type.\n");
			ST(0)  = &PL_sv_undef;
		} else {
			parse_headers(msg, ~0, 0);
			if (getType(msg) == SIP_REQUEST) {
				firsttoken = (msg->first_line).u.request.method.s;
			} else { /* SIP_REPLY */
				firsttoken = (msg->first_line).u.reply.version.s;
			}

			if (msg->eoh == NULL)
				headerlen = 0;
			else
				headerlen = ((long)(msg->eoh))
						-((long)(firsttoken));

			if (headerlen > 0) {
				ST(0) = 
				    sv_2mortal(newSVpv(firsttoken, headerlen));
			} else {
				ST(0) = &PL_sv_undef;
			}
		}
	}


=head2 getBody()

Returns the message body.

=cut

SV *
getBody(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		parse_headers(msg, ~0, 0);
		ST(0) = sv_2mortal(newSVpv(get_body(msg), 0));
	}


=head2 getMessage()

Returns the whole message including headers and body.

=cut

SV *
getMessage(self)
    SV *self
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		ST(0) = sv_2mortal(newSVpv(msg->buf, 0));
	}


=head2 getHeader(name)

Returns the body of the first message header with this name.

C<< print $m->getHeader("To"); >>

B<C<< "John" <sip:john@doe.example> >>>

=cut

SV *
getHeader(self, name)
    SV *self;
    char *name;
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    str *body = NULL;
    struct hdr_field *hf;
    int found = 0;
    int namelen = strlen(name);
  INIT:
  PPCODE:
	LM_DBG("searching '%s'\n", name);

	if (!msg) {
		LM_ERR("Invalid message reference\n");
	} else {
		parse_headers(msg, ~0, 0);
		for (hf = msg->headers; hf; hf = hf->next) {
			if (namelen == hf->name.len) {
				if (strncmp(name, hf->name.s, namelen) == 0) {
					/* Found the right header. */
					found = 1;
					body = &(hf->body);
					XPUSHs(sv_2mortal(newSVpv(body->s,
								  body->len)));
				}
			}
		}
	}
	if (!found) {
		XPUSHs(&PL_sv_undef);
	}



=head2 getHeaderNames()

Returns an array of all header names. Duplicates possible!

=cut

AV *
getHeaderNames(self)
    SV *self;
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    struct hdr_field *hf = NULL;
    int found = 0;
  PPCODE:
	
	if (!msg) {
		LM_ERR("Invalid message reference\n");
	} else {
		parse_headers(msg, ~0, 0);
		for (hf = msg->headers; hf; hf = hf->next) {
			found = 1;
			XPUSHs(sv_2mortal(newSVpv(hf->name.s, hf->name.len)));
		}
	}
	if (!found) {
		XPUSHs(&PL_sv_undef);
	}


=head2 moduleFunction(func,string1,string2)

Search for an arbitrary function in module exports and call it with the
parameters self, string1, string2.

C<string1> and/or C<string2> may be omitted.

As this function provides access to the functions that are exported to the
Kamailio configuration file, it is autoloaded for unknown functions. Instead of
writing

 $m->moduleFunction("sl_send_reply", "500", "Internal Error");
 $m->moduleFunction("xlog", "L_INFO", "foo");
 
you may as well write

 $m->sl_send_reply("500", "Internal Error");
 $m->xlog("L_INFO", "foo");

WARNING

In Kamailio 1.2, only a limited subset of module functions is available. This
restriction will be removed in a later version.

Here is a list of functions that are expected to be working (not claiming
completeness):

 * alias_db_lookup
 * consume_credentials
 * is_rpid_user_e164
 * append_rpid_hf
 * bind_auth
 * avp_print
 * cpl_process_register
 * cpl_process_register_norpl
 * load_dlg
 * ds_next_dst
 * ds_next_domain
 * ds_mark_dst
 * ds_mark_dst
 * is_from_local
 * is_uri_host_local
 * dp_can_connect
 * dp_apply_policy
 * enum_query (without parameters)
 * enum_fquery (without parameters)
 * is_from_user_enum (without parameters)
 * i_enum_query (without parameters)
 * imc_manager
 * jab_* (all functions from the jabber module)
 * load_gws (without parameters)
 * next_gw
 * from_gw (without parameters)
 * to_gw (without parameters)
 * sdp_mangle_ip
 * sdp_mangle_port
 * encode_contact
 * decode_contact
 * decode_contact_header
 * fix_contact
 * use_media_proxy
 * end_media_session
 * m_store
 * m_dump
 * fix_nated_contact
 * unforce_rtp_proxy
 * force_rtp_proxy
 * fix_nated_register
 * add_rcv_param
 * options_reply
 * checkospheader
 * validateospheader
 * requestosprouting
 * checkosproute
 * prepareosproute
 * prepareallosproutes
 * checkcallingtranslation
 * reportospusage
 * mangle_pidf
 * mangle_message_cpim
 * add_path (without parameters)
 * add_path_received (without parameters)
 * prefix2domain
 * allow_routing (without parameters)
 * allow_trusted
 * pike_check_req
 * handle_publish
 * handle_subscribe
 * stored_pres_info
 * bind_pua
 * send_publish
 * send_subscribe
 * pua_set_publish
 * loose_route
 * record_route
 * load_rr
 * sip_trace
 * sl_reply_error
 * sms_send_msg
 * sd_lookup
 * sstCheckMin
 * append_time
 * has_body (without parameters)
 * is_peer_verified
 * t_newtran
 * t_release
 * t_relay (without parameters)
 * t_flush_flags
 * t_check_trans
 * t_was_cancelled
 * t_load_contacts
 * t_next_contacts
 * uac_restore_from
 * uac_auth
 * has_totag
 * tel2sip
 * check_to
 * check_from
 * radius_does_uri_exist
 * ul_* (All functions exported by the usrloc module for user access)
 * xmpp_send_message

=cut


int
moduleFunction (self, func, string1 = NULL, string2 = NULL)
    SV *self;
    char *func;
    char *string1;
    char *string2;
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    int retval; /* Return value of called function */
    int ret;    /* Return value of moduleFunc - < 0 for "non existing function" and other errors */
  INIT:
  CODE:
	LM_DBG("Calling exported func '%s', Param1 is '%s',"
		" Param2 is '%s'\n", func, string1, string2);

	ret = moduleFunc(msg, func, string1, string2, &retval);
	if (ret < 0) {
		LM_ERR("calling module function '%s' failed."
			" Missing loadmodule?\n", func);
		retval = -1;
	}
	RETVAL = retval;
  OUTPUT:
	RETVAL



=head2 log(level,message) (deprecated type)

Logs the message with Kamailio's logging facility. The logging level
is one of the following:

 * L_ALERT
 * L_CRIT
 * L_ERR
 * L_WARN
 * L_NOTICE
 * L_INFO
 * L_DBG

The logging function should be accessed via the Kamailio module variant. This
one, located in Kamailio::Message, is deprecated.

=cut

void
log(self, level, log)
    SV *self
    int level
    char *log
  PREINIT:
  INIT:
  CODE:
	switch (level) {
	case L_ALERT:	LM_ALERT("%s", log); break;
	case L_CRIT:	LM_CRIT("%s", log); break;
	case L_ERR:	LM_ERR("%s", log); break;
	case L_WARN:	LM_WARN("%s", log); break;
	case L_NOTICE:	LM_NOTICE("%s", log); break;
	case L_INFO:	LM_INFO("%s", log); break;
	default:	LM_DBG("%s", log); break;
	}



=head2 rewrite_ruri(newruri)

Sets a new destination (recipient) URI. Useful for rerouting the
current message/call.

 if ($m->getRURI() =~ m/\@somedomain.net/) {
   $m->rewrite_ruri("sip:dispatcher\@organization.net");
 }

=cut

int
rewrite_ruri(self, newruri)
    SV *self;
    char *newruri;
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		RETVAL = -1;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LM_ERR("Not a Request. RURI rewrite unavailable.\n");
			RETVAL = -1;
		} else {
			LM_DBG("New R-URI is [%s]\n", newruri);
			RETVAL = rewrite_ruri(msg, newruri);
		}
	}
  OUTPUT:
	RETVAL



=head2 setFlag(flag)

Sets a message flag. The constants as known from the C API may be used,
when Constants.pm is included.

=cut

int
setFlag(self, flag)
    SV *self;
    unsigned int flag;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		RETVAL = -1;
	} else {
		RETVAL = setflag(msg, flag);
	}
  OUTPUT:
	RETVAL


=head2 resetFlag(flag)

Resets a message flag.

=cut

int
resetFlag(self, flag)
    SV *self;
    unsigned int flag;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		RETVAL = -1;
	} else {
		RETVAL = resetflag(msg, flag);
	}
  OUTPUT:
	RETVAL

=head2 isFlagSet(flag)

Returns whether a message flag is set or not.

=cut

int
isFlagSet(self, flag)
    SV *self;
    unsigned int flag;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
  INIT:
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		RETVAL = -1;
	} else {
		RETVAL = isflagset(msg, flag) == 1 ? 1 : 0;
	}
  OUTPUT:
	RETVAL


=head2 pseudoVar(string)

Returns a new string where all pseudo variables are substituted by their values.
Can be used to receive the values of single variables, too.

B<Please remember that you need to escape the '$' sign in perl strings!>

=cut

SV *
pseudoVar(self, varstring)
    SV *self;
    char *varstring;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
	char *ret;
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		ret = pv_sprintf(msg, varstring);
		if (ret) {
			ST(0) = sv_2mortal(newSVpv(ret, strlen(ret)));
			free(ret);
		} else {
			ST(0) = &PL_sv_undef;
		}
	}



=head2 append_branch(branch,qval)

Append a branch to current message.

=cut

int
append_branch(self, branch = NULL, qval = NULL)
	SV *self;
	char *branch;
	char *qval;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
	qvalue_t q = Q_UNSPECIFIED;
	str b = {0, 0};
  INIT:
  CODE:
  	if (!msg) {
		LM_ERR("Invalid message reference\n");
		RETVAL = -1;
	} else {
		if (qval) {
			if (str2q(&q, qval, strlen(qval)) < 0) {
				LM_ERR("append_branch: Bad q value.");
			} else { /* branch and qval set */
				b.s = branch;
				b.len = strlen(branch);
			}
		} else {
			if (branch) { /* branch set, qval unset */
				b.s = branch;
				b.len = strlen(branch);
			}
		}

		RETVAL = km_append_branch(msg, (b.s!=0)?&b:0, 0, 0, q, 0, 0);
	}
  OUTPUT:
	RETVAL



=head2 getParsedRURI()

Returns the current destination URI as an Kamailio::URI object.

=cut

SV *
getParsedRURI(self)
    SV *self;
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    struct sip_uri *uri;
    SV *ret;
  INIT:
  CODE:
	if (!msg) {
		LM_ERR("Invalid message reference\n");
		ST(0) = NULL;
	} else {
		parse_sip_msg_uri(msg);
		parse_headers(msg, ~0, 0);

		uri = &(msg->parsed_uri);
		ret = sv_newmortal();
		sv_setref_pv(ret, "Kamailio::URI", (void *)uri);
		SvREADONLY_on(SvRV(ret));

		ST(0) = ret;
	}
	


MODULE = Kamailio PACKAGE = Kamailio::URI

=head1 Kamailio::URI

This package provides functions for access to sip_uri structures.

=cut




=head2 user()

Returns the user part of this URI.

=cut

SV *
user(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_USER);


=head2 host()

Returns the host part of this URI.

=cut

SV *
host(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_HOST);


=head2 passwd()

Returns the passwd part of this URI.

=cut

SV *
passwd(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_PASSWD);


=head2 port()

Returns the port part of this URI.

=cut

SV *
port(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_PORT);


=head2 params()

Returns the params part of this URI.

=cut

SV *
params(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_PARAMS);


=head2 headers()

Returns the headers part of this URI.

=cut

SV *
headers(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_HEADERS);


=head2 transport()

Returns the transport part of this URI.

=cut

SV *
transport(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_TRANSPORT);


=head2 ttl()

Returns the ttl part of this URI.

=cut

SV *
ttl(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_TTL);


=head2 user_param()

Returns the user_param part of this URI.

=cut

SV *
user_param(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_USER_PARAM);



=head2 maddr()

Returns the maddr part of this URI.

=cut

SV *
maddr(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_MADDR);

=head2 method()

Returns the method part of this URI.

=cut

SV *
method(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_METHOD);


=head2 lr()

Returns the lr part of this URI.

=cut

SV *
lr(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_LR);


=head2 r2()

Returns the r2 part of this URI.

=cut

SV *
r2(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_R2);


=head2 transport_val()

Returns the transport_val part of this URI.

=cut

SV *
transport_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_TRANSPORT_VAL);


=head2 ttl_val()

Returns the ttl_val part of this URI.

=cut

SV *
ttl_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_TTL_VAL);


=head2 user_param_val()

Returns the user_param_val part of this URI.

=cut

SV *
user_param_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_USER_PARAM_VAL);


=head2 maddr_val()

Returns the maddr_val part of this URI.

=cut

SV *
maddr_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_MADDR_VAL);


=head2 method_val()

Returns the method_val part of this URI.

=cut

SV *
method_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_METHOD_VAL);


=head2 lr_val()

Returns the lr_val part of this URI.

=cut

SV *
lr_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_LR_VAL);


=head2 r2_val()

Returns the r2_val part of this URI.

=cut

SV *
r2_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, XS_URI_R2_VAL);



=head1 Kamailio::AVP

This package provides access functions for Kamailio's AVPs.
These variables can be created, evaluated, modified and removed through this
package.

Please note that these functions do NOT support the notation used
in the configuration file, but directly work on strings or numbers. See
documentation of add method below.

=cut


MODULE = Kamailio PACKAGE = Kamailio::AVP

=head2 add(name,val)

Add an AVP.

Add an Kamailio AVP to its environment. name and val may both be integers or
strings; this function will try to guess what is correct. Please note that
 
 Kamailio::AVP::add("10", "10")

is something different than

 Kamailio::AVP::add(10, 10)

due to this evaluation: The first will create _string_ AVPs with the name
10, while the latter will create a numerical AVP.

You can modify/overwrite AVPs with this function.

=cut

int
add(p_name, p_val)
	SV *p_name;
	SV *p_val;
  PREINIT:
	int_str name;
	int_str val;
	unsigned short flags = 0;
	char *s;
	STRLEN len;
  CODE:
  	RETVAL = 0;
	if (SvOK(p_name) && SvOK(p_val)) {
		if (!sv2int_str(p_name, &name, &flags, AVP_NAME_STR)) {
			RETVAL = -1;
		} else if (!sv2int_str(p_val, &val, &flags, AVP_VAL_STR)) {
			RETVAL = -1;
		}

		if (RETVAL == 0) {
			RETVAL = add_avp(flags, name, val);
		}
	}
  OUTPUT:
	RETVAL




=head2 get(name)

get an Kamailio AVP:

 my $numavp = Kamailio::AVP::get(5);
 my $stravp = Kamailio::AVP::get("foo");

=cut

int
get(p_name)
	SV *p_name;
  PREINIT:
	struct usr_avp *first_avp;
	int_str name;
	int_str val;
	unsigned short flags = 0;
	SV *ret = &PL_sv_undef;
	int err = 0;
	char *s;
	STRLEN len;
  CODE:
	if (SvOK(p_name)) {
		if (!sv2int_str(p_name, &name, &flags, AVP_NAME_STR)) {
			LM_ERR("AVP:get: Invalid name.");
			err = 1;
		}
	} else {
		LM_ERR("AVP:get: Invalid name.");
		err = 1;
	}
	
	if (err == 0) {
		first_avp = search_first_avp(flags, name, &val, NULL);
		
		if (first_avp != NULL) { /* found correct AVP */
			if (is_avp_str_val(first_avp)) {
				ret = sv_2mortal(newSVpv(val.s.s, val.s.len));
			} else {
				ret = sv_2mortal(newSViv(val.n));
			}
		} else {
			/* Empty AVP requested. */
		}
	}

	ST(0) = ret;




=head2 destroy(name)

Destroy an AVP.

 Kamailio::AVP::destroy(5);
 Kamailio::AVP::destroy("foo");

=cut

int
destroy(p_name)
	SV *p_name;
  PREINIT:
	struct usr_avp *first_avp;
	int_str name;
	int_str val;
	unsigned short flags = 0;
	SV *ret = &PL_sv_undef;
	char *s;
	STRLEN len;
  CODE:
	RETVAL = 1;
	if (SvOK(p_name)) {
		if (!sv2int_str(p_name, &name, &flags, AVP_NAME_STR)) {
			RETVAL = 0;
			LM_ERR("AVP:destroy: Invalid name.");
		}
	} else {
		RETVAL = 0;
		LM_ERR("VP:destroy: Invalid name.");
	}
	
	if (RETVAL == 1) {
		first_avp = search_first_avp(flags, name, &val, NULL);
		
		if (first_avp != NULL) { /* found correct AVP */
			destroy_avp(first_avp);
		} else {
			RETVAL = 0;
			/* Empty AVP requested. */
		}
	}

  OUTPUT:
	RETVAL


