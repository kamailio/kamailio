/*
 * GRUU extraction and lookup implementation
 */

#include "gruu.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/str.h"
#include "../../core/ut.h"
#include <string.h>
#include <stdio.h>

extern usrloc_api_t ul;

#define GRUU_DECODE_BUF 512

int param_eq(param_t *p, const char *name)
{
	if(!p || !name)
		return 0;
	if((int)strlen(name) != p->name.len)
		return 0;
	return (strncasecmp(p->name.s, name, p->name.len) == 0);
}

static void gruu_fields_free(gruu_fields_t *gf)
{
	if(!gf)
		return;
	if(gf->instance_id.s)
		pkg_free(gf->instance_id.s);
	if(gf->pub_gruu.s)
		pkg_free(gf->pub_gruu.s);
	if(gf->temp_gruu.s)
		pkg_free(gf->temp_gruu.s);
	memset(gf, 0, sizeof(*gf));
}

static int gruu_dup_str(const str *src, str *dst)
{
	if(!src || !src->s || src->len <= 0) {
		dst->s = NULL;
		dst->len = 0;
		return 0;
	}
	dst->s = (char *)pkg_malloc(src->len + 1);
	if(!dst->s)
		return -1;
	memcpy(dst->s, src->s, src->len);
	dst->s[src->len] = '\0';
	dst->len = src->len;
	return 0;
}

int pcscf_gruu_fields_dup(gruu_fields_t *gf, const str *instance_id,
		const str *pub_gruu, const str *temp_gruu)
{
	if(!gf)
		return -1;
	memset(gf, 0, sizeof(*gf));

	if(instance_id && instance_id->len > 0) {
		if(gruu_dup_str(instance_id, &gf->instance_id) < 0)
			goto error;
	}
	if(pub_gruu && pub_gruu->len > 0) {
		if(gruu_dup_str(pub_gruu, &gf->pub_gruu) < 0)
			goto error;
	}
	if(temp_gruu && temp_gruu->len > 0) {
		if(gruu_dup_str(temp_gruu, &gf->temp_gruu) < 0)
			goto error;
	}

	if(gf->instance_id.len == 0 && gf->pub_gruu.len == 0
			&& gf->temp_gruu.len == 0)
		return -1;
	return 0;
error:
	gruu_fields_free(gf);
	return -1;
}

int extract_gruu_from_contact(contact_t *c, gruu_fields_t *gf)
{
	param_t *p;
	if(!c || !gf)
		return -1;
	memset(gf, 0, sizeof(*gf));

	/* sip.instance hook */
	if(c->instance && c->instance->body.len > 0) {
		/* strip surrounding quotes if present */
		char *s = c->instance->body.s;
		int len = c->instance->body.len;
		if(len >= 2 && s[0] == '"' && s[len - 1] == '"') {
			len -= 2;
			s = s + 1;
		}
		gf->instance_id.s = (char *)pkg_malloc(len + 1);
		if(!gf->instance_id.s) {
			gruu_fields_free(gf);
			return -1;
		}
		memcpy(gf->instance_id.s, s, len);
		gf->instance_id.s[len] = '\0';
		gf->instance_id.len = len;
	}

	/* scan generic params for pub-gruu and temp-gruu */
	for(p = c->params; p; p = p->next) {
		if(!p->body.s || p->body.len == 0)
			continue;
		if(param_eq(p, "pub-gruu")) {
			gf->pub_gruu.s = (char *)pkg_malloc(p->body.len + 1);
			if(!gf->pub_gruu.s) {
				gruu_fields_free(gf);
				return -1;
			}
			memcpy(gf->pub_gruu.s, p->body.s, p->body.len);
			gf->pub_gruu.s[p->body.len] = '\0';
			gf->pub_gruu.len = p->body.len;
		} else if(param_eq(p, "temp-gruu")) {
			gf->temp_gruu.s = (char *)pkg_malloc(p->body.len + 1);
			if(!gf->temp_gruu.s) {
				gruu_fields_free(gf);
				return -1;
			}
			memcpy(gf->temp_gruu.s, p->body.s, p->body.len);
			gf->temp_gruu.s[p->body.len] = '\0';
			gf->temp_gruu.len = p->body.len;
		}
	}

	/* return 0 if any field populated, otherwise -1 */
	if((gf->instance_id.s && gf->instance_id.len > 0)
			|| (gf->pub_gruu.s && gf->pub_gruu.len > 0)
			|| (gf->temp_gruu.s && gf->temp_gruu.len > 0))
		return 0;
	gruu_fields_free(gf);
	return -1;
}

int pcscf_apply_gruu(udomain_t *d, pcontact_t *c, gruu_fields_t *gf)
{
	int ret = -1;
	if(!d || !c || !gf)
		return -1;
	if(ul.update_contact_gruu == NULL) {
		LM_ERR("update_contact_gruu API not available\n");
		return -1;
	}

	/* call update_contact_gruu only (do not call save_temp_gruu_history) */
	ret = ul.update_contact_gruu(d, c,
			(gf->instance_id.len ? &gf->instance_id : NULL),
			(gf->pub_gruu.len ? &gf->pub_gruu : NULL),
			(gf->temp_gruu.len ? &gf->temp_gruu : NULL));

	/* free temp allocations */
	gruu_fields_free(gf);
	return ret;
}

static int pcscf_decode_gruu_param(
		str *gr, str *gruu_dec, char *buf, int buf_len)
{
	int i, j, out_len;

	if(!gr || !gr->s || gr->len <= 0 || !gruu_dec || !buf || buf_len <= 0)
		return -1;

	out_len = gr->len;
	for(i = 0; i + 2 < gr->len; i++) {
		if(gr->s[i] == '%' && gr->s[i + 1] == '3'
				&& (gr->s[i + 2] == 'B' || gr->s[i + 2] == 'b'))
			out_len -= 2;
	}
	if(out_len + 1 > buf_len)
		return -1;

	for(i = 0, j = 0; i < gr->len; i++) {
		if(i + 2 < gr->len && gr->s[i] == '%' && gr->s[i + 1] == '3'
				&& (gr->s[i + 2] == 'B' || gr->s[i + 2] == 'b')) {
			buf[j++] = ';';
			i += 2;
			continue;
		}
		buf[j++] = gr->s[i];
	}
	buf[j] = '\0';
	gruu_dec->s = buf;
	gruu_dec->len = j;
	return 0;
}

int pcscf_gruu_resolve_contact(
		udomain_t *d, struct sip_msg *m, pcontact_t **c_out)
{
	str gruu_dec = {0, 0};
	char decode_buf[GRUU_DECODE_BUF];
	pcontact_t *c = NULL;

	if(!d || !m || !c_out)
		return -1;
	*c_out = NULL;

	if(parse_sip_msg_uri(m) < 0)
		return -1;
	if(m->parsed_uri.gr.len <= 0 || !m->parsed_uri.gr.s)
		return -1;

	if(pcscf_decode_gruu_param(
			   &m->parsed_uri.gr, &gruu_dec, decode_buf, sizeof(decode_buf))
			!= 0)
		return -1;

	if(ul.get_pcontact_by_pub_gruu
			&& ul.get_pcontact_by_pub_gruu(d, &gruu_dec, &c) == 0 && c) {
		*c_out = c;
		return 1;
	}
	if(ul.get_pcontact_by_temp_gruu
			&& ul.get_pcontact_by_temp_gruu(d, &gruu_dec, &c) == 0 && c) {
		*c_out = c;
		return 1;
	}
	return -1;
}

int pcscf_gruu_lookup(struct sip_msg *m, udomain_t *d)
{
	pcontact_t *c = NULL;

	if(!m || !d)
		return -1;
	if(pcscf_gruu_resolve_contact(d, m, &c) != 1 || !c)
		return -1;
	if(set_dst_uri(m, &c->aor) != 0)
		return -1;
	return 1;
}
