/* $Id$ */

#ifndef TRIM_H
#define TRIM_H

#include "str.h"


/*
 * This switch-case statement is used in
 * trim_leading and trim_trailing. You can
 * define char that should be skipped here.
 */
#define TRIM_SWITCH(c) switch(c) {     \
                       case ' ':       \
                       case '\t':      \
                       case '\r':      \
                       case '\n':      \
                               break;  \
                                       \
                       default:        \
                               return; \
                       }


/*
 * Remove any leading whitechars, like spaces,
 * horizontal tabs, carriage returns and line
 * feeds
 *
 * WARNING: String descriptor structure will be
 *          modified ! Make a copy otherwise you
 *          might be unable to free _s->s for
 *          example !
 *
 */
static inline void trim_leading(str* _s)
{
	for(; _s->len > 0; _s->len--, _s->s++) {
		TRIM_SWITCH(*(_s->s));
	}
}


/*
 * Remove any trailing white char, like spaces,
 * horizontal tabs, carriage returns and line feeds
 *
 * WARNING: String descriptor structure will be
 *          modified ! Make a copy otherwise you
 *          might be unable to free _s->s for
 *          example !
 */
static inline void trim_trailing(str* _s)
{
	for(; _s->len > 0; _s->len--) {
		TRIM_SWITCH(_s->s[_s->len - 1]);
	}
}


/*
 * Do trim_leading and trim_trailing
 *
 * WARNING: String structure will be modified !
 *          Make a copy otherwise you might be
 *          unable to free _s->s for example !
 */
static inline void trim(str* _s)
{
	trim_leading(_s);
	trim_trailing(_s);
}


#endif /* TRIM_H */
