/* 
 * $Id$ 
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include "../../msg_parser.h"

char*             trim                (char* _s);
char*             eat_lws             (char* _b);
struct hdr_field* remove_crlf         (struct hdr_field* _hf);
char*             strlower            (char* _s, int len);
char*             find_not_quoted     (char* _b, char c);
struct hdr_field* duplicate_hf        (const struct hdr_field* _hf);
void              free_hf             (struct hdr_field* _hf);
char*             find_lws            (char* _s);
char*             find_not_quoted_lws (char* _s);

#endif
