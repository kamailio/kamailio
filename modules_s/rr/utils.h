/*
 * $Id$ 
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "../../msg_parser.h"

#define PARANOID


#ifndef TRUE
#define TRUE 0
#endif

#ifndef FALSE
#define FALSE 1
#endif

#define SUCCESS 0
#define FAILURE 1


char* trim(char* _s);
char* eat_lws(char* _b);
struct hdr_field* remove_crlf(struct hdr_field* _hf);

char* strlower(char* _s, int len);
char* strupper(char* _s, int len);

char* parse_to_char(char* _to);

char* find_not_quoted(char* _b, char c);
char* eat_name(char* _b);

#endif
