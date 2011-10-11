/*$Id$
 *
 * Copyright (C) 2011 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mem/mem.h"
#include "str.h"
#include "sr_module.h"
#include "msg_shooter_mod.h"
#include "smsg_routes.h"
#include "msg_shooter.h"

/* method points to a static char buffer, all of the others are
dinamically allocated */
static str	method = {0, 0};
static str	from = {0, 0};
static str	to = {0, 0};
static str	hdrs = {0, 0};
static str	body = {0, 0};
static dlg_t	*UAC = 0;
/* onreply route index */
static int	onreply_idx = -1;

/* get method of the request */
int smsg_create(struct sip_msg *_msg, char *_param1, char *_param2)
{
	/* check the required information */
	if (!from.len || !to.len) {
		LOG(L_ERR, "ERROR: smsg_create(): mandatory headers are missing\n");
		LOG(L_ERR, "ERROR: smsg_create(): have you forgot to call smsg_from_to() function?\n");
		return -1;
	}

	if (get_str_fparam(&method, _msg, (fparam_t*)_param1)) {
		LOG(L_ERR, "ERROR: smsg_create(): cannot get parameter\n");
		return -1;
	}
	/* method is just a static char buffer, needless to copy it */

	/* previous UAC still exists -- destroy it first */
	if (UAC) {
		LOG(L_DBG, "DEBUG: smsg_create(): destroying previous UAC\n");
		tmb.free_dlg(UAC);
		UAC = 0;
		/* better to free also hdrs and body now */
		if (hdrs.s) {
			pkg_free(hdrs.s);
			hdrs.s = 0;
			hdrs.len = 0;
		}
		if (body.s) {
			pkg_free(body.s);
			body.s = 0;
			body.len = 0;
		}
	}

	/* create UAC */
	if (tmb.new_dlg_uac(0, 0, 0, &from, &to, &UAC) < 0) {
		LOG(L_ERR, "ERROR: smsg_create(): cannot create UAC\n");
		return -1;
	}
	return 1;
}

/* free allocated memory */
void smsg_destroy(void)
{
	method.s = 0;
	method.len = 0;
	if (from.s) {
		pkg_free(from.s);
		from.s = 0;
		from.len = 0;
	}
	if (to.s) {
		pkg_free(to.s);
		to.s = 0;
		to.len = 0;
	}
	if (hdrs.s) {
		pkg_free(hdrs.s);
		hdrs.s = 0;
		hdrs.len = 0;
	}
	if (body.s) {
		pkg_free(body.s);
		body.s = 0;
		body.len = 0;
	}
	if (UAC) {
		tmb.free_dlg(UAC);
		UAC = 0;
	}
	onreply_idx = -1;
}

/* clone an str structure */
static int clone_str(str *_s, str *_d)
{
	if (_d->s) pkg_free(_d->s);

	if (_s->len == 0) {
		/* empty string */
		_d->s = 0;
		_d->len = 0;
		return 0;
	}

	_d->s = (char *)pkg_malloc(_s->len * sizeof(char));
	if (!_d->s) {
		LOG(L_ERR, "ERROR: clone_str(): not enough memory\n");
		return -1;
	}
	memcpy(_d->s, _s->s, _s->len);
	_d->len = _s->len;

	return 0;
}

/* set From and To headers of the request */
int smsg_from_to(struct sip_msg *_msg, char *_param1, char *_param2)
{
	str	s;

	if (get_str_fparam(&s, _msg, (fparam_t*)_param1)) {
		LOG(L_ERR, "ERROR: smsg_from_to(): cannot get parameter\n");
		return -1;
	}
	/* select and AVP result can change, we need a private copy of the buffer */
	if (clone_str(&s, &from)) return -1;

	if (get_str_fparam(&s, _msg, (fparam_t*)_param2)) {
		LOG(L_ERR, "ERROR: smsg_from_to(): cannot get parameter\n");
		return -1;
	}
	/* select and AVP result can change, we need a private copy of the buffer */
	if (clone_str(&s, &to)) return -1;

	return 1;
}

/* append headers and optionally body to the request */
int smsg_append_hdrs(struct sip_msg *_msg, char *_param1, char *_param2)
{
	str	s;

	if (get_str_fparam(&s, _msg, (fparam_t*)_param1)) {
		LOG(L_ERR, "ERROR: smsg_append_hdrs(): cannot get parameter\n");
		return -1;
	}
	/* select and AVP result can change, we need a private copy of the buffer */
	if (clone_str(&s, &hdrs)) return -1;

	if (_param2) {
		if (get_str_fparam(&s, _msg, (fparam_t*)_param2)) {
			LOG(L_ERR, "ERROR: smsg_append_hdrs(): cannot get parameter\n");
			return -1;
		}
		/* select and AVP result can change, we need a private copy of the buffer */
		if (clone_str(&s, &body)) return -1;
	} else {
		if (body.s) {
			pkg_free(body.s);
			body.s = 0;
			body.len = 0;
		}
	}

	return 1;
}

/*
 * callback function for TM module
 * it is called on TMCB_LOCAL_COMPLETED
 */
static void tmcb_func(struct cell *_t, int _type, struct tmcb_params *_ps)
{
	int	index;

	if (_type & (TMCB_LOCAL_COMPLETED)) {
		if ((!_ps->rpl) || (_ps->rpl == FAKED_REPLY)) {
			/* timer hit  -- on_failure route is not supported */
			LOG(L_DBG, "DEBUG: tmcb_func(): transaction completed with failure (timer hit),"
				" but msg_shooter module does not support failure_route currently\n");
		} else {
			/* reply received */
			if (_ps->code >= 400) {
				LOG(L_DBG, "DEBUG: tmcb_func(): transaction completed with failure (code=%d),"
					" but msg_shooter module does not support failure_route currently\n",
					_ps->code);
			}
			if (!_ps->param) {
				LOG(L_ERR, "ERROR: tmcb_func(): parameter is missing\n");
				return;		
			}
			index = (int)(long)(*_ps->param);
			run_reply_route(_ps->rpl, _t, index);
		}
	}
}


static avp_list_t def_avp_list = 0;

/* shoots a request to a destination outside of a dialog */
int smsg(struct sip_msg *_msg, char *_param1, char *_param2)
{
	int	ret = 1;
	str	ruri = {0, 0};
	str	dst = {0, 0};
	avp_list_t	*backup_uri_from, *backup_uri_to;
	avp_list_t	*backup_user_from, *backup_user_to;
	avp_list_t	*backup_domain_from, *backup_domain_to;
	uac_req_t	uac_r;

	/* check the required information */
	if (!UAC) {
		LOG(L_ERR, "ERROR: smsg(): UAC is missing\n");
		LOG(L_ERR, "ERROR: smsg(): have you forgot to call smsg_from_to() and smsg_create() functions?\n");
		return -1;
	}

	if (_param1 && get_str_fparam(&ruri, _msg, (fparam_t*)_param1)) {
		LOG(L_ERR, "ERROR: smsg(): cannot get parameter\n");
		return -1;
	}

	if (_param2 && get_str_fparam(&dst, _msg, (fparam_t*)_param2)) {
		LOG(L_ERR, "ERROR: smsg(): cannot get parameter\n");
		return -1;
	}

	LOG(L_DBG, "DEBUG: smsg(): sending %.*s request "
			"(from=%.*s, to=%.*s, ruri=%.*s, dst=%.*s)\n",
			method.len, method.s,
			from.len, from.s,
			to.len, to.s,
			ruri.len, ruri.s,
			dst.len, dst.s);


	if (ruri.len) {
		if (tmb.set_dlg_target(UAC, &ruri, &dst) < 0) {
			LOG(L_ERR, "ERROR: smsg(): cannot set remote target\n");
			return -1;
		}
	}

	/* reset user AVP lists, otherwise TM would free the memory twice cousing crash */
	backup_uri_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &def_avp_list);
	backup_uri_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &def_avp_list);
	backup_user_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &def_avp_list);
	backup_user_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &def_avp_list);
	backup_domain_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &def_avp_list);
	backup_domain_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &def_avp_list);

	set_uac_req(&uac_r,
			&method,
			(hdrs.len) ? &hdrs : 0,
			(body.len) ? &body : 0,
			UAC,
			(onreply_idx < 0) ? 0 : TMCB_LOCAL_COMPLETED,
			(onreply_idx < 0) ? 0 : tmcb_func,
			(onreply_idx < 0) ? 0 : (void *)(long)onreply_idx
		);

	if (tmb.t_uac(&uac_r) < 0) {
		LOG(L_ERR, "ERROR: smsg(): request could not be sent\n");
		ret = -1;
	}

	/* restore AVP lists */
	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, backup_uri_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, backup_uri_to);
	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, backup_user_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, backup_user_to);
	set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, backup_domain_from);
	set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, backup_domain_to);

	/* reset smsg_on_reply */
	onreply_idx = -1;
	return ret;
}

/* sents on_reply route which will be called later */
int smsg_on_reply(struct sip_msg *_msg, char *_param1, char *_param2)
{
	onreply_idx = (int)(long)(_param1);
	return 1;
}
