#ifndef _MSFUNCS_H_
#define _MSFUNCS_H_

#include "../../str.h"

#define CT_TYPE		1
#define CT_CHARSET	2
#define CT_MSGR		4

#ifdef MSILO_TAG
#undef MSILO_TAG
#endif
#define MSILO_TAG	"msilo-HI4U-Ah0X-bZ98-"

typedef struct _content_type
{
	str type;
	str charset;
	str msgr;
} t_content_type;

/** apostrophes escape - useful for MySQL strings */
int apo_escape(char*, int, char*, int);

/** parse content-type header - only for TYPE is noe implemented */
int parse_content_type(char*, int, t_content_type*, int);

/** sends IM using IM library */
int m_send_message(int mid, str *uri, str *to, str *from, str *contact,
				str *ctype, str *msg);

/** build MESSAGE headers */
int m_build_headers(str *buf, str ctype);

/** build MESSAGE body */
int m_build_body(str *body, str from, int date, str msg);

#endif

