/*
 * $Id$
 *
 * Supported header field parser macros
 */

#ifndef CASE_SUPP_H
#define CASE_SUPP_H


#define ORTE_CASE                                  \
        switch(val) {                              \
        case _orte_:                               \
	        p += 4;                            \
	        if ((*p == 'd') || (*p == 'D')) {  \
		        hdr->type = HDR_SUPPORTED; \
                        p++;                       \
                        goto dc_end;               \
                }                                  \
                goto other;                        \
        }


#define Supp_CASE         \
        p += 4;           \
        val = READ(p);    \
        ORTE_CASE;        \
                          \
        val = unify(val); \
        ORTE_CASE;        \
        goto other;


#endif /* CASE_SUPP_H */
