/*
 * $Id$
 *
 * Unsupported header field parser macros
 */

#ifndef CASE_UNSU_H
#define CASE_UNSU_H


#define TED_CASE                             \
        switch(val) {                        \
        case _ted1_:                         \
                hdr->type = HDR_UNSUPPORTED; \
                hdr->name.len = 11;          \
                *(p + 3) = '\0';             \
	        return (p + 4);              \
                                             \
        case _ted2_:                         \
                hdr->type = HDR_UNSUPPORTED; \
                p += 4;                      \
	        goto dc_end;                 \
        }


#define PPOR_CASE                 \
        switch(val) {             \
        case _ppor_:              \
                p += 4;           \
                val = READ(p);    \
                TED_CASE;         \
                                  \
                val = unify(val); \
                TED_CASE;         \
                goto other;       \
        }


#define Unsu_CASE         \
        p += 4;           \
        val = READ(p);    \
        PPOR_CASE;        \
                          \
        val = unify(val); \
        PPOR_CASE;        \
        goto other;       \


#endif /* CASE_UNSU_H */
