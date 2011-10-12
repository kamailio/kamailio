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

#include "action.h"
#include "route.h"
#include "modules/tm/h_table.h"
#include "smsg_routes.h"

/* run reply route functions */
int run_reply_route(struct sip_msg *_rpl, struct cell *_t, int index)
{
	avp_list_t	*backup_uri_from, *backup_uri_to;
	avp_list_t	*backup_user_from, *backup_user_to;
	avp_list_t	*backup_domain_from, *backup_domain_to;
	struct run_act_ctx	ra_ctx;

	if (!_t || (index < 0)) return -1;

	/* set the avp_list the one from transaction */
	backup_uri_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_URI, &_t->uri_avps_from );
	backup_uri_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_URI, &_t->uri_avps_to );
	backup_user_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_USER, &_t->user_avps_from );
	backup_user_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_USER, &_t->user_avps_to );
	backup_domain_from = set_avp_list(AVP_TRACK_FROM | AVP_CLASS_DOMAIN, &_t->domain_avps_from );
	backup_domain_to = set_avp_list(AVP_TRACK_TO | AVP_CLASS_DOMAIN, &_t->domain_avps_to );

	init_run_actions_ctx(&ra_ctx);
	if (run_actions(&ra_ctx, onreply_rt.rlist[index], _rpl)<0)
		LOG(L_ERR, "ERROR: run_reply_route(): on_reply processing failed\n");

	/* restore original avp list */
	set_avp_list( AVP_TRACK_FROM | AVP_CLASS_URI, backup_uri_from );
	set_avp_list( AVP_TRACK_TO | AVP_CLASS_URI, backup_uri_to );
	set_avp_list( AVP_TRACK_FROM | AVP_CLASS_USER, backup_user_from );
	set_avp_list( AVP_TRACK_TO | AVP_CLASS_USER, backup_user_to );
	set_avp_list( AVP_TRACK_FROM | AVP_CLASS_DOMAIN, backup_domain_from );
	set_avp_list( AVP_TRACK_TO | AVP_CLASS_DOMAIN, backup_domain_to );

	return 0;
}
