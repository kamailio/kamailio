/*
 * $Id$
 *
 * From header field parser macros
 */

#ifndef CASE_FROM_H
#define CASE_FROM_H


#define From_CASE             \
        hdr->type = HDR_FROM; \
        p += 4;               \
        goto dc_end


#endif /* CASE_FROM_H */
