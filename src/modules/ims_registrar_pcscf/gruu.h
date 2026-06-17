/*
 * GRUU extraction and lookup API for ims_registrar_pcscf
 */
#ifndef IMS_REGISTRAR_PCSCF_GRUU_H
#define IMS_REGISTRAR_PCSCF_GRUU_H

#include "../../core/parser/contact/contact.h"
#include "../ims_usrloc_pcscf/usrloc.h"
#include "../../core/str.h"

typedef struct
{
	str instance_id;
	str pub_gruu;
	str temp_gruu;
} gruu_fields_t;

/* Extracts GRUU and instance parameters from a parsed Contact.
 * Returns 0 if any field was extracted, -1 on error or nothing found. */
int extract_gruu_from_contact(contact_t *c, gruu_fields_t *gf);

/* Copy field strings into pkg-owned gruu_fields_t (for NOTIFY/libxml sources). */
int pcscf_gruu_fields_dup(gruu_fields_t *gf, const str *instance_id,
		const str *pub_gruu, const str *temp_gruu);

/* Apply GRUU fields to a pcontact via usrloc API (calls update_contact_gruu only). */
int pcscf_apply_gruu(udomain_t *d, pcontact_t *c, gruu_fields_t *gf);

/* Resolve GRUU in Request-URI to a contact. Returns 1 on match, -1 otherwise. */
int pcscf_gruu_resolve_contact(udomain_t *d, struct sip_msg *m, pcontact_t **c);

/* Lookup GRUU in Request-URI and set dst-uri to contact AOR. Returns 1/-1. */
int pcscf_gruu_lookup(struct sip_msg *m, udomain_t *d);

/* helper for parameter name comparison (exposed for unit tests) */
int param_eq(param_t *p, const char *name);

#endif
