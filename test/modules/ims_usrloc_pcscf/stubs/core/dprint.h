/* Test-only stand-in for src/core/dprint.h.
 *
 * Production ims_usrloc_pcscf sources no longer guard their real includes
 * behind #ifdef UNIT_TEST. The real dprint.h pulls in cfg_core.h and the
 * rest of the Kamailio core config framework, which a standalone unit
 * test binary cannot link against. This header is only ever reached via
 * the staged copy of the production .c file (see stubs/modules/) whose
 * relative "../../core/dprint.h" include resolves here instead of the
 * real core header.
 */
#ifndef PCSCF_STUB_DPRINT_H
#define PCSCF_STUB_DPRINT_H

#include <stdio.h>

#define LM_ALERT(...) fprintf(stderr, __VA_ARGS__)
#define LM_CRIT(...) fprintf(stderr, __VA_ARGS__)
#define LM_ERR(...) fprintf(stderr, __VA_ARGS__)
#define LM_WARN(...) fprintf(stderr, __VA_ARGS__)
#define LM_NOTICE(...) fprintf(stderr, __VA_ARGS__)
#define LM_INFO(...) fprintf(stderr, __VA_ARGS__)
#define LM_DBG(...) ((void)0)

#endif /* PCSCF_STUB_DPRINT_H */
