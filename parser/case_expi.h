/*
 * $Id$
 *
 * Expires header field parser macros
 */

#ifndef CASE_EXPI_H
#define CASE_EXPI_H


#define EXPI_RES_CASE                    \
        switch(val) {                    \
        case _res1_:                     \
		hdr->type = HDR_EXPIRES; \
		hdr->name.len = 7;       \
                *(p + 3) = '\0';         \
		return (p + 4);          \
                                         \
        case _res2_:                     \
		hdr->type = HDR_EXPIRES; \
		p += 4;                  \
		goto dc_end;             \
        }


#define Expi_CASE         \
        p += 4;           \
        val = READ(p);    \
        EXPI_RES_CASE;    \
                          \
        val = unify(val); \
        EXPI_RES_CASE;    \
        goto other;


#endif
