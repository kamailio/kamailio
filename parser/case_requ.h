/*
 * $Id$
 *
 * Require header field parser macros
 */

#ifndef CASE_REQU_H
#define CASE_REQU_H


#define IRE_CASE                         \
        switch(val) {                    \
        case _ire1_:                     \
                hdr->type = HDR_REQUIRE; \
                hdr->name.len = 7;       \
                *(p + 3) = '\0';         \
                return (p + 4);          \
                                         \
        case _ire2_:                     \
                hdr->type = HDR_REQUIRE; \
                p += 4;                  \
                goto dc_end;             \
        }


#define Requ_CASE         \
        p += 4;           \
        val = READ(p);    \
        IRE_CASE;         \
                          \
        val = unify(val); \
        IRE_CASE;         \
        goto other;


#endif /* CASE_REQU_H */
