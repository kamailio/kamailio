/*
 * $Id$
 *
 * To header field parser macros
 */

#ifndef CASE_TO_H
#define CASE_TO_H


#define To12_CASE           \
        hdr->type = HDR_TO; \
        hdr->name.len = 2;  \
        *(p + 2) = '\0';    \
        return (p + 4);


#endif
