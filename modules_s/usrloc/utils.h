/* 
 * $Id$ 
 */

#ifndef UTILS_H
#define UTILS_H

#include "../../parser/msg_parser.h"

#define PARANOID


#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define SUCCESS 1
#define FAILURE 0

/*
 * Remove any leading white chars
 */
char* trim_leading(char* _s);

/*
 * Remove any trailing white chars
 */
char* trim_trailing(char* _s);

/*
 * Remove all leading and trailing white chars
 */
char* trim(char* _s);


/*
 * Eat linear white space
 */
char* eat_lws(char* _b);


/*
 * Substitute \r or \n with spaces
 */
struct hdr_field* remove_crlf(struct hdr_field* _hf);

/*
 * Convert string to lower case
 */
char* strlower(char* _s, int len);

/*
 * Convert string to upper case
 */
char* strupper(char* _s, int len);

/*
 * Find a character that is not quoted
 */
char* find_not_quoted(char* _b, char c);

/*
 * Skip the name part of a URL if any
 */
char* eat_name(char* _b);

#endif
