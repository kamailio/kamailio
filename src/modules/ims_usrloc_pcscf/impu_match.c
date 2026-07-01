/* impu_match.c */
#include "impu_match.h"
#include <string.h>

#ifdef UNIT_TEST
/* minimal sip_uri declaration for unit tests to avoid heavy parser includes */
struct sip_uri
{
	int type;
	str user;
	str host;
};
int parse_uri(char *buf, int len, struct sip_uri *uri);
#else
#include "../../core/parser/parse_uri.h"
#endif

int pcscf_impu_matches_aor(str *aor, str *impu)
{
	struct sip_uri aor_uri, impu_uri;

	if(!aor || !aor->s || !impu || !impu->s)
		return 0;
	if(aor->len == impu->len && strncasecmp(aor->s, impu->s, aor->len) == 0)
		return 1;
	if(parse_uri(aor->s, aor->len, &aor_uri) != 0)
		return 0;
	if(parse_uri(impu->s, impu->len, &impu_uri) != 0)
		return 0;
	if(aor_uri.user.len && impu_uri.user.len
			&& aor_uri.user.len == impu_uri.user.len
			&& strncasecmp(aor_uri.user.s, impu_uri.user.s, aor_uri.user.len)
					   == 0)
		return 1;
	return 0;
}

int pcscf_contact_has_impu(pcontact_t *c, str *aor)
{
	ppublic_t *p;
	if(!c || !aor)
		return 0;
	if(pcscf_impu_matches_aor(aor, &c->aor))
		return 1;
	for(p = c->head; p; p = p->next)
		if(pcscf_impu_matches_aor(aor, &p->public_identity))
			return 1;
	return 0;
}

int is_impu_barred(pcontact_t *c, str *impu)
{
	ppublic_t *p;

	if(!c || !impu)
		return 0;
	for(p = c->head; p; p = p->next) {
		if(p->barred && pcscf_impu_matches_aor(impu, &p->public_identity))
			return 1;
	}
	return 0;
}
