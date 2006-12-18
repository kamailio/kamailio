/*
 * $Id$
 *
 * Perl module for OpenSER
 *
 * Copyright (C) 2006 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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
 */

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include <unistd.h>
#undef load_module
#include "../../sr_module.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../action.h"
#include "../../flags.h"
#include "../../items.h"
#include "../../mem/mem.h"
#include "../../route_struct.h"
#include "../../serialize.h"
#include "../../qvalue.h"


enum uri_members {
	user = 0,
	passwd,
	host,
	port,
	params,
	headers,
	transport,
	ttl,
	user_param,
	maddr,
	method,
	lr,
	r2,
	transport_val,
	ttl_val,
	user_param_val,
	maddr_val,
	method_val,
	lr_val,
	r2_val
	
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
		

SV *getStringFromURI(SV *self, enum uri_members what) {
	struct sip_uri *myuri = sv2uri(self);
	str *ret = NULL;

	if (!myuri) {
		LOG(L_ERR, "perl: Invalid URI reference\n");
		ret = NULL;
	} else {
		
		switch (what) {
			case user:		ret = &(myuri->user);
						break;
			case host:		ret = &(myuri->host);
						break;
			case passwd:		ret = &(myuri->passwd);
						break;
			case port:		ret = &(myuri->port);
						break;
			case params:		ret = &(myuri->params);
						break;
			case headers:		ret = &(myuri->headers);
						break;
			case transport:		ret = &(myuri->transport);
						break;
			case ttl:		ret = &(myuri->ttl);
						break;
			case user_param:	ret = &(myuri->user_param);
						break;
			case maddr:		ret = &(myuri->maddr);
						break;
			case method:		ret = &(myuri->method);
						break;
			case lr:		ret = &(myuri->lr);
						break;
			case r2:		ret = &(myuri->r2);
						break;
			case transport_val:	ret = &(myuri->transport_val);
						break;
			case ttl_val:		ret = &(myuri->ttl_val);
						break;
			case user_param_val:	ret = &(myuri->user_param_val);
						break;
			case maddr_val:		ret = &(myuri->maddr_val);
						break;
			case method_val:	ret = &(myuri->method_val);
						break;
			case lr_val:		ret = &(myuri->lr_val);
						break;
			case r2_val:		ret = &(myuri->r2_val);
						break;

			default:	LOG(L_INFO, "Unknown URI element"
						" requested: %d\n", what);
					break;
		}
	}

	if (ret) {
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
 *    1 - Function was called. It's return value is returned via the retval
 *        parameter.
 */

int moduleFunc(struct sip_msg *m, char *func,
	       char *param1, char *param2,
	       int *retval) {

	cmd_export_t *exp_func_struct;
	struct action *act;
	char *argv[2];
	int argc = 0;

	if (!func) {
		LOG(L_ERR, "moduleFunc called with null function name. Error.");
		return -1;
	}

	if ((!param1) && param2) {
		LOG(L_ERR, "moduleFunc called with parameter 1 UNSET and"
			   " parameter 2 SET. Error.");
		return -1;
	}


	if (param1) {
		argv[0] = (char *)pkg_malloc(strlen(param1)+1);
		strcpy(argv[0], param1);
		argc++;
	} else {
		argv[0] = NULL;
	}

	if (param2) {
		argv[1] = (char *)pkg_malloc(strlen(param2)+1);
		strcpy(argv[1], param2);
		argc++;
	} else {
		argv[1] = NULL;
	}

	exp_func_struct = find_cmd_export_t(func, argc, 0);
	if (!exp_func_struct) {
		LOG(L_ERR, "function '%s' called, but not available.", func);
		*retval = -1;
		return -1;
	}

	act = mk_action_3p(	MODULE_T,
				CMD_ST,
				STRING_ST,
				STRING_ST,
				exp_func_struct,
				argv[0],
				argv[1],
				0);


	if (!act) {
		LOG(L_ERR, "action structure could not be created. Error.");
		if (argv[0]) pkg_free(argv[0]);
		if (argv[1]) pkg_free(argv[1]);
		return -1;
	}


	if (exp_func_struct->fixup) {
		if (argc>=2) {
			*retval = exp_func_struct->fixup(&(act->p3.data), 2);
			if (*retval < 0) {
				LOG(L_ERR, "Error in fixup (2)\n");
				return -1;
			}
			act->p3_type = MODFIXUP_ST;
		}
		if (argc>=1) {
			*retval = exp_func_struct->fixup(&(act->p2.data), 1);
			if (*retval < 0) {
				LOG(L_ERR, "Error in fixup (1)\n");
				return -1;
			}
			act->p2_type = MODFIXUP_ST;
		}
		if (argc==0) {
			*retval = exp_func_struct->fixup(&(act->p1.data), 0);
			if (*retval < 0) {
				LOG(L_ERR, "Error in fixup (0)\n");
				return -1;
			}
		}
	}

	*retval = do_action(act, m);

	if ((act->p3_type == STRING_ST) && (act->p3.string)) {
		pkg_free(act->p3.string);
	}
	
	if ((act->p2_type == STRING_ST) && (act->p2.string)) {
		pkg_free(act->p2.string);
	}
	pkg_free(act);
	
	return 1;
}


/**
 * Rewrite Request-URI
 */
static inline int rewrite_ruri(struct sip_msg* _m, char* _s)
{
	struct action act;

	act.type = SET_URI_T;
	act.p1_type = STRING_ST;
	act.p1.string = _s;
	act.next = 0;
	
	if (do_action(&act, _m) < 0)
	{
		LOG(L_ERR, "perl:rewrite_ruri: Error in do_action\n");
		return -1;
	}
	return 0;
}


/**
 * Compile a string with pseudo variables substituted by their values.
 * A string buffer is allocated. Deallocate afterwards!
 */
char *xl_sprintf(struct sip_msg *m, char *fmt) {
	int buf_size = 4096;
	xl_elem_t *model;

	char *out = (char *)pkg_malloc(buf_size);
	char *ret = NULL;

	if (!out) {
		LOG(L_ERR, "perl:xl_sprintf: Memory exhausted!\n");
		return NULL;
	}

	if(xl_parse_format(fmt, &model, XL_DISABLE_NONE) < 0) {
		LOG(L_ERR, "perl:xl_sprintf: ERROR: wrong format[%s]!\n",
			fmt);
		return NULL;
	}

	if(xl_printf(m, model, out, &buf_size) < 0) {
		ret = NULL;
	} else {
		ret = strdup(out);
	}
	pkg_free(out);

	return ret;
}

/* ************************************************************************ */
/* Object methods begin here */

=head1 OpenSER

This module provides access to a limited number of OpenSER core functions.
As the most interesting functions deal with SIP messages, they are located
in the OpenSER::Message class below.

=cut

MODULE = OpenSER PACKAGE = OpenSER

=head2 log(level,message)

Logs the message with OpenSER's logging facility. The logging level
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
import the function (via C<use OpenSER qw ( log );>), or call it with it's full
name:

 OpenSER::log(L_INFO, "foobar");

=cut

void
log(level, log)
    int level
    char *log
  PREINIT:
  INIT:
  CODE:
	LOG(level, "%s", log);
  OUTPUT:



MODULE = OpenSER PACKAGE = OpenSER::Message

PROTOTYPES: ENABLE

=head1 OpenSER::Message

This package provides access functions for an OpenSER C<sip_msg> structure and
it's sub-components. Through it's means it is possible to fully configure
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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REPLY) {
			LOG(L_ERR, "perl:getStatus: Status not available in"
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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REPLY) {
			LOG(L_ERR, "perl:getReason: Reason not available in"
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
		LOG(L_ERR, "perl: Invalid message reference\n");
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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LOG(L_ERR, "perl: Not a request message - "
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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LOG(L_ERR, "perl: Not a request message - "
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
    SV *ret;
    char *firsttoken;
  INIT:
  CODE:
	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		if (getType(msg) == SIP_INVALID) {
			LOG(L_ERR, "perl:getFullHeader: Invalid message type.\n");
			ST(0)  = &PL_sv_undef;
		} else {
			if (getType(msg) == SIP_REQUEST) {
				parse_headers(msg, ~0, 0);
				firsttoken = (msg->first_line).u.request.method.s;
			} else { /* SIP_REPLY */
				firsttoken = (msg->first_line).u.reply.version.s;
			}

			ret = newSVpv(firsttoken,
				(((long)(msg->eoh))-
				 ((long)(firsttoken))));
			ST(0) = sv_2mortal(ret);
		}
	}


=head2 getBody()

Returns the message body.

=cut

SV *
getBody(self)
    SV *self
  PREINIT:
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    SV *ret;
  INIT:
  CODE:
	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
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
  PREINIT:
    struct sip_msg *msg = sv2msg(self);
    SV *ret;
  INIT:
  CODE:
	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
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
	DBG("getHeader: searching '%s'\n", name);
	
	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
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
		LOG(L_ERR, "perl: Invalid message reference\n");
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
OpenSER configuration file, it is autoloaded for unknown functions. Instead of
writing

 $m->moduleFunction("sl_send_reply", "500", "Internal Error");
 $m->moduleFunction("xlog", "L_INFO", "foo");
 
you may as well write

 $m->sl_send_reply("500", "Internal Error");
 $m->xlog("L_INFO", "foo");

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
	LOG(L_DBG, "perl: Calling exported func '%s', Param1 is '%s',"
		" Param2 is '%s'\n", func, string1, string2);

	ret = moduleFunc(msg, func, string1, string2, &retval);
	if (ret < 0) {
		LOG(L_ERR, "perl: calling module function '%s' failed."
			" Missing loadmodule?\n", func);
		retval = -1;
	}
	RETVAL = retval;
  OUTPUT:
	RETVAL



=head2 log(level,message) (deprecated type)

Logs the message with OpenSER's logging facility. The logging level
is one of the following:

 * L_ALERT
 * L_CRIT
 * L_ERR
 * L_WARN
 * L_NOTICE
 * L_INFO
 * L_DBG

The logging function should be accessed via the OpenSER module variant. This
one, located in OpenSER::Message, is deprecated.

=cut

void
log(self, level, log)
    SV *self
    int level
    char *log
  PREINIT:
  INIT:
  CODE:
	LOG(level, "%s", log);



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
		LOG(L_ERR, "perl: Invalid message reference\n");
		RETVAL = -1;
	} else {
		if (getType(msg) != SIP_REQUEST) {
			LOG(L_ERR, "perl:rewrite_ruri: Not a Request. "
				"RURI rewrite unavailable.\n");
			RETVAL = -1;
		} else {
			DBG("perl:rewrite_ruri: New R-URI is [%s]\n", newruri);
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
		LOG(L_ERR, "perl: Invalid message reference\n");
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
		LOG(L_ERR, "perl: Invalid message reference\n");
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
		LOG(L_ERR, "perl: Invalid message reference\n");
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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = &PL_sv_undef;
	} else {
		ret = xl_sprintf(msg, varstring);
		if (ret) {
			ST(0) = sv_2mortal(newSVpv(ret, strlen(ret)));
			pkg_free(ret);
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
	qvalue_t q;
	int err = 0;
	struct action *act = NULL;
  INIT:
  CODE:
  	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
		RETVAL = -1;
	} else {
		if (qval) {
			if (str2q(&q, qval, strlen(qval)) < 0) {
				LOG(L_ERR, "perl:append_branch: Bad q value.");
			} else { /* branch and qval set */
				act = mk_action_2p(APPEND_BRANCH_T,
						STRING_ST,
						NUMBER_ST,
						branch,
						(void *)(long)q, 0);
			}
		} else {
			if (branch) { /* branch set, qval unset */
				act = mk_action_2p(APPEND_BRANCH_T,
						STRING_ST,
						NUMBER_ST,
						branch,
						(void *)Q_UNSPECIFIED, 0);
			} else { /* neither branch nor qval set */
				act = mk_action_2p(APPEND_BRANCH_T,
						STRING_ST,
						NUMBER_ST,
						NULL,
						(void *)Q_UNSPECIFIED, 0);
			}
		}

		if (act) {
			RETVAL = do_action(act, msg);
		} else {
			RETVAL = -1;
		}
	}
  OUTPUT:
	RETVAL



=head2 serialize_branches(clean_before)

Serialize branches.

=cut

int serialize_branches(self, clean_before)
	SV *self;
	int clean_before;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
  CODE:
  	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
		RETVAL = -1;
	} else {
		RETVAL = serialize_branches(msg, clean_before);
	}
  OUTPUT:
	RETVAL



=head2 next_branches()

Next branches.

=cut

int
next_branches(self)
	SV *self;
  PREINIT:
	struct sip_msg *msg = sv2msg(self);
  CODE:
  	if (!msg) {
		LOG(L_ERR, "perl: Invalid message reference\n");
		RETVAL = -1;
	} else {
		RETVAL = next_branches(msg);
	}
  OUTPUT:
	RETVAL




=head2 getParsedRURI()

Returns the current destination URI as an OpenSER::URI object.

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
		LOG(L_ERR, "perl: Invalid message reference\n");
		ST(0) = NULL;
	} else {
		parse_sip_msg_uri(msg);
		parse_headers(msg, ~0, 0);

		uri = &(msg->parsed_uri);
		ret = sv_newmortal();
		sv_setref_pv(ret, "OpenSER::URI", (void *)uri);
		SvREADONLY_on(SvRV(ret));

		ST(0) = ret;
	}
	


MODULE = OpenSER PACKAGE = OpenSER::URI

=head1 OpenSER::URI

This package provides functions for access to sip_uri structures.

=cut




=head2 user()

Returns the user part of this URI.

=cut

SV *
user(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, user);


=head2 host()

Returns the host part of this URI.

=cut

SV *
host(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, host);


=head2 passwd()

Returns the passwd part of this URI.

=cut

SV *
passwd(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, passwd);


=head2 port()

Returns the port part of this URI.

=cut

SV *
port(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, port);


=head2 params()

Returns the params part of this URI.

=cut

SV *
params(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, params);


=head2 headers()

Returns the headers part of this URI.

=cut

SV *
headers(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, headers);


=head2 transport()

Returns the transport part of this URI.

=cut

SV *
transport(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, transport);


=head2 ttl()

Returns the ttl part of this URI.

=cut

SV *
ttl(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, ttl);


=head2 user_param()

Returns the user_param part of this URI.

=cut

SV *
user_param(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, user_param);



=head2 maddr()

Returns the maddr part of this URI.

=cut

SV *
maddr(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, maddr);

=head2 method()

Returns the method part of this URI.

=cut

SV *
method(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, method);


=head2 lr()

Returns the lr part of this URI.

=cut

SV *
lr(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, lr);


=head2 r2()

Returns the r2 part of this URI.

=cut

SV *
r2(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, r2);


=head2 transport_val()

Returns the transport_val part of this URI.

=cut

SV *
transport_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, transport_val);


=head2 ttl_val()

Returns the ttl_val part of this URI.

=cut

SV *
ttl_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, ttl_val);


=head2 user_param_val()

Returns the user_param_val part of this URI.

=cut

SV *
user_param_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, user_param_val);


=head2 maddr_val()

Returns the maddr_val part of this URI.

=cut

SV *
maddr_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, maddr_val);


=head2 method_val()

Returns the method_val part of this URI.

=cut

SV *
method_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, method_val);


=head2 lr_val()

Returns the lr_val part of this URI.

=cut

SV *
lr_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, lr_val);


=head2 r2_val()

Returns the r2_val part of this URI.

=cut

SV *
r2_val(self)
    SV *self;
  CODE:
	ST(0) = getStringFromURI(self, r2_val);


