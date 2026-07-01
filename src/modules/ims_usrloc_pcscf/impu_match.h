/* impu_match.h */
#ifndef IMPU_MATCH_H
#define IMPU_MATCH_H
#include "../../core/str.h"

#ifdef UNIT_TEST
typedef struct ppublic
{
	str public_identity;
	int barred;
	struct ppublic *next;
} ppublic_t;

typedef struct pcontact
{
	str aor;
	ppublic_t *head;
	ppublic_t *tail;
} pcontact_t;
#else
#include "usrloc.h"
#endif

int pcscf_impu_matches_aor(str *aor, str *impu);
int pcscf_contact_has_impu(pcontact_t *c, str *aor);
int is_impu_barred(pcontact_t *c, str *impu);
#endif
