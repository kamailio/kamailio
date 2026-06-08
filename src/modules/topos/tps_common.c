/**
 * Shared topos helpers linked into topos_htable / topos_redis
 */

#include "../../core/parser/parse_expires.h"
#include "../../core/parser/contact/parse_contact.h"
#include "../../core/parser/parse_sipifmatch.h"
#include "../../core/strutils.h"
#include "../../core/trim.h"

#include "tps_storage.h"

/**
 * Fill expires / expires_valid from Expires header or Contact;expires=
 */
int tps_data_is_reg_pub(unsigned int method_id)
{
	return (method_id & (METHOD_REGISTER | METHOD_PUBLISH)) ? 1 : 0;
}

void tps_data_fill_expires(sip_msg_t *msg, tps_data_t *td, contact_t *ct)
{
	unsigned int e;
	int hdr_expires = -1;
	int ct_expires = -1;

	if(td->s_method_id != METHOD_SUBSCRIBE && td->s_method_id != METHOD_REGISTER
			&& td->s_method_id != METHOD_PUBLISH) {
		return;
	}

	td->expires_valid = 0;

	if(parse_headers(msg, HDR_EXPIRES_F, 0) != -1 && msg->expires
			&& msg->expires->body.len > 0
			&& (msg->expires->parsed || parse_expires(msg->expires) >= 0)) {
		hdr_expires = (int)((exp_body_t *)msg->expires->parsed)->val;
	}

	if(ct == NULL) {
		if(parse_headers(msg, HDR_CONTACT_F, 0) >= 0 && msg->contact != NULL
				&& parse_contact(msg->contact) >= 0) {
			ct = ((contact_body_t *)msg->contact->parsed)->contacts;
		}
	}
	if(ct != NULL && ct->expires != NULL && ct->expires->body.len > 0
			&& str2int(&ct->expires->body, &e) >= 0) {
		ct_expires = (int)e;
	}

	/* Contact ;expires=0 deregister wins over a non-zero Expires header */
	if(hdr_expires == 0 || ct_expires == 0) {
		td->expires = 0;
		td->expires_valid = 1;
		return;
	}
	if(hdr_expires >= 0) {
		td->expires = hdr_expires;
		td->expires_valid = 1;
		return;
	}
	if(ct_expires >= 0) {
		td->expires = ct_expires;
		td->expires_valid = 1;
	}
}

/**
 * Whether dialog storage should be ended (BYE, failed setup, Expires=0)
 */
int tps_data_end_dialog_match(sip_msg_t *msg, tps_data_t *md)
{
	if(msg == NULL || md == NULL) {
		return 0;
	}

	if(md->s_method_id == METHOD_BYE) {
		return 1;
	}

	if(msg->first_line.type == SIP_REPLY
			&& msg->first_line.u.reply.statuscode > 299
			&& (get_cseq(msg)->method_id
					& (METHOD_INVITE | METHOD_SUBSCRIBE | METHOD_REGISTER
							| METHOD_PUBLISH))) {
		return 1;
	}

	if((md->s_method_id == METHOD_SUBSCRIBE
			   || md->s_method_id == METHOD_REGISTER
			   || md->s_method_id == METHOD_PUBLISH)
			&& md->expires_valid && md->expires == 0) {
		return 1;
	}

	return 0;
}

/**
 * Htable/redis dialog key TTL from message expires or module default
 */
int tps_data_dialog_expire_ttl(tps_data_t *td, int default_expire)
{
	if((td->s_method_id == METHOD_SUBSCRIBE || td->s_method_id == METHOD_REGISTER
			   || td->s_method_id == METHOD_PUBLISH)
			&& td->expires_valid && td->expires > 0) {
		return td->expires;
	}
	return default_expire;
}

/**
 * Initial REGISTER/PUBLISH (CSeq 1, not deregister)
 */
int tps_reg_pub_is_initial(tps_data_t *td)
{
	unsigned int cseq = 0;

	if(td == NULL) {
		return 0;
	}
	if(td->expires_valid && td->expires == 0) {
		return 0;
	}
	if(td->s_cseq.len > 0 && str2int(&td->s_cseq, &cseq) >= 0) {
		return (cseq <= 1) ? 1 : 0;
	}
	return 1;
}

/**
 * SIP-If-Match body (RFC 3903 PUBLISH refresh)
 */
int tps_data_get_sipifmatch(sip_msg_t *msg, str *etag)
{
	if(msg == NULL || etag == NULL) {
		return -1;
	}
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		return -1;
	}
	if(msg->sipifmatch == NULL) {
		return 1;
	}
	if(msg->sipifmatch->parsed == NULL) {
		if(parse_sipifmatch(msg->sipifmatch) < 0) {
			return -1;
		}
	}
	*etag = *((str *)msg->sipifmatch->parsed);
	return 0;
}

/**
 * SIP-ETag header from a response (RFC 3903)
 */
int tps_data_get_sipetag_hdr(sip_msg_t *msg, str *etag)
{
	struct hdr_field *hf;
	static str hname = STR_STATIC_INIT("SIP-ETag");

	if(msg == NULL || etag == NULL) {
		return -1;
	}
	if(parse_headers(msg, HDR_EOH_F, 0) == -1) {
		return -1;
	}
	for(hf = msg->headers; hf != NULL; hf = hf->next) {
		if(cmp_hdrname_str(&hf->name, &hname) == 0) {
			etag->s = hf->body.s;
			etag->len = hf->body.len;
			trim(etag);
			if(etag->len > 0) {
				return 0;
			}
			return -1;
		}
	}
	return 1;
}
