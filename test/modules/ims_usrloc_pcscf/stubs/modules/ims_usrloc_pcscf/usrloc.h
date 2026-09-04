/* Test-only stand-in for src/modules/ims_usrloc_pcscf/usrloc.h.
 *
 * The real header pulls in ul_callback.h, qvalue.h, tm/dlg.h, cdp headers
 * and core/counters.h, none of which a standalone unit test binary can
 * link against. This provides just the pcontact_t / ppublic_t / udomain_t
 * fields that pcontact_index.c and impu_match.c actually read.
 * Reached only via the staged copies of those production .c/.h files
 * (see this same stubs/modules/ims_usrloc_pcscf/ directory), whose bare
 * "usrloc.h" include resolves here instead of the real module header.
 */
#ifndef PCSCF_STUB_USRLOC_H
#define PCSCF_STUB_USRLOC_H

#include <stdlib.h>
#include "pcontact_index.h"

#ifndef shm_malloc
#define shm_malloc(s) malloc(s)
#endif
#ifndef shm_free
#define shm_free(p) free(p)
#endif

typedef struct ppublic
{
	str public_identity;
	char barred;
	struct ppublic *next;
	struct ppublic *prev;
} ppublic_t;

struct pcontact
{
	str aor;
	str pub_gruu;
	str temp_gruu;
	ppublic_t *head;
	ppublic_t *tail;
};

struct udomain
{
	pcscf_index_t impu_idx;
	pcscf_index_t pub_gruu_idx;
	pcscf_index_t temp_gruu_idx;
};
typedef struct udomain udomain_t;

#endif /* PCSCF_STUB_USRLOC_H */
