/*
 * $Id$
 *
 * Allow header field parser macros
 */

#ifndef CASE_ALLO_H
#define CASE_ALLO_H


#define Allo_CASE                     \
    p += 4;                           \
    if ((*p == 'w') || (*p == 'W')) { \
            hdr->type = HDR_ALLOW;    \
            p++;                      \
	    goto dc_end;              \
    }                                 \
    goto other;


#endif
