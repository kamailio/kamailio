/*
 * $Id$
 *
 * Event header field parser macros
 */

#ifndef CASE_EVEN_H
#define CASE_EVEN_H


#define Even_CASE                     \
    p += 4;                           \
    if ((*p == 't') || (*p == 'T')) { \
            hdr->type = HDR_EVENT;    \
            p++;                      \
	    goto dc_end;              \
    }                                 \
    goto other;


#endif /* CASE_EVEN_H */
