/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sys/types.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>

#include "parser/parse_uri.h"
#include "parser/parse_param.h"

#include "dprint.h"
#include "ut.h"
#include "strutils.h"

/*! \brief
 * add backslashes to special characters
 */
int escape_common(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	for(i=0; i<src_len; i++)
	{
		switch(src[i])
		{
			case '\'':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '"':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\\':
				dst[j++] = '\\';
				dst[j++] = src[i];
				break;
			case '\0':
				dst[j++] = '\\';
				dst[j++] = '0';
				break;
			default:
				dst[j++] = src[i];
		}
	}
	return j;
}

/*! \brief
 * remove backslashes to special characters
 */
int unescape_common(char *dst, char *src, int src_len)
{
	int i, j;

	if(dst==0 || src==0 || src_len<=0)
		return 0;
	j = 0;
	i = 0;
	while(i<src_len)
	{
		if(src[i]=='\\' && i+1<src_len)
		{
			switch(src[i+1])
			{
				case '\'':
					dst[j++] = '\'';
					i++;
					break;
				case '"':
					dst[j++] = '"';
					i++;
					break;
				case '\\':
					dst[j++] = '\\';
					i++;
					break;
				case '0':
					dst[j++] = '\0';
					i++;
					break;
				default:
					dst[j++] = src[i];
			}
		} else {
			dst[j++] = src[i];
		}
		i++;
	}
	return j;
}

/*! \brief Unscape all printable ASCII characters */
int unescape_user(str *sin, str *sout)
{
	char *at, *p, c;

	if(sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL
			|| sin->len<0 || sout->len < sin->len+1)
		return -1;

	at = sout->s;
	p  = sin->s;
	while(p < sin->s+sin->len)
	{
	    if (*p == '%')
		{
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c = (*p - '0') << 4;
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = (*p - 'a' + 10) << 4;
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = (*p - 'A' + 10) << 4;
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c =  c + (*p - '0');
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = c + (*p - 'a' + 10);
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = c + (*p - 'A' + 10);
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			if ((c < 32) || (c > 126))
			{
			    LM_ERR("invalid escaped character <%u>\n", (unsigned int)c);
			    return -1;
			}
			*at++ = c;
	    } else {
			*at++ = *p;
	    }
		p++;
	}

	*at = 0;
	sout->len = at - sout->s;

	LM_DBG("unescaped string is <%s>\n", sout->s);
	return 0;
}

/*! \brief
 * Escape all printable characters that are not valid in user
 * part of request uri
 * no_need_to_escape = unreserved | user-unreserved
 * unreserved = aplhanum | mark
 * mark = - | _ | . | ! | ~ | * | ' | ( | )
 * user-unreserved = & | = | + | $ | , | ; | ? | /
 */
int escape_user(str *sin, str *sout)
{

	char *at, *p;
	unsigned char x;

	if(sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL
			|| sin->len<0 || sout->len < 3*sin->len+1)
		return -1;


	at = sout->s;
	p  = sin->s;
	while (p < sin->s+sin->len)
	{
	    if (*p < 32 || *p > 126)
		{
			LM_ERR("invalid escaped character <%u>\n", (unsigned int)*p);
			return -1;
	    }
	    if (isdigit((int)*p) || ((*p >= 'A') && (*p <= 'Z')) ||
				((*p >= 'a') && (*p <= 'z')))
		{
			*at = *p;
	    } else {
			switch (*p) {
				case '-':
				case '_':
				case '.':
				case '!':
				case '~':
				case '*':
				case '\'':
				case '(':
				case ')':
				case '&':
				case '=':
				case '+':
				case '$':
				case ',':
				case ';':
				case '?':
				    *at = *p;
				break;
				default:
				    *at++ = '%';
				    x = (*p) >> 4;
				    if (x < 10)
					{
						*at++ = x + '0';
				    } else {
						*at++ = x - 10 + 'a';
				    }
				    x = (*p) & 0x0f;
				    if (x < 10) {
						*at = x + '0';
				    } else {
						*at = x - 10 + 'a';
				    }
			}
	    }
	    at++;
	    p++;
	}
	*at = 0;
	sout->len = at - sout->s;
	LM_DBG("escaped string is <%s>\n", sout->s);
	return 0;
}


int unescape_param(str *sin, str *sout)
{
    return unescape_user(sin, sout);
}


/*! \brief
 * Escape all printable characters that are not valid in
 * a param part of request uri
 * no_need_to_escape = unreserved | param-unreserved
 * unreserved = alphanum | mark
 * mark = - | _ | . | ! | ~ | * | ' | ( | )
 * param-unreserved = [ | ] | / | : | & | + | $
 */
int escape_param(str *sin, str *sout)
{
    char *at, *p;
    unsigned char x;

    if (sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL ||
        sin->len<0 || sout->len < 3*sin->len+1)
        return -1;

    at = sout->s;
    p  = sin->s;
    while (p < sin->s+sin->len) {
        if (*p < 32 || *p > 126) {
            LM_ERR("invalid escaped character <%u>\n", (unsigned int)*p);
            return -1;
        } else if (isdigit((int)*p) || ((*p >= 'A') && (*p <= 'Z')) ||
                ((*p >= 'a') && (*p <= 'z'))) {
            *at = *p;
        } else {
            switch (*p) {
                case '-':
                case '_':
                case '.':
                case '!':
                case '~':
                case '*':
                case '\'':
                case '(':
                case ')':
                case '[':
                case ']':
                case '/':
                case ':':
                case '&':
                case '+':
                case '$':
                    *at = *p;
                    break;
                default:

                    *at++ = '%';
                    x = (*p) >> 4;
                    if (x < 10)
                    {
                        *at++ = x + '0';
                    } else {
                        *at++ = x - 10 + 'a';
                    }
                    x = (*p) & 0x0f;
                    if (x < 10) {
                        *at = x + '0';
                    } else {
                        *at = x - 10 + 'a';
                    }
                    break;
            }
        }
        at++;
        p++;
    }
    *at = 0;
    sout->len = at - sout->s;
    LM_DBG("escaped string is <%s>\n", sout->s);

    return 0;
}

/*! \brief
 * escapes a string to use as a CSV field, as specified in RFC4180:
 * - enclose sting in double quotes
 * - escape double quotes with a second double quote
 */
int escape_csv(str *sin, str *sout)
{
    char *at, *p;

    if (sin==NULL || sout==NULL || sin->s==NULL || sout->s==NULL ||
        sin->len<0 || sout->len < 2*sin->len+3)
        return -1;

    at = sout->s;
    p  = sin->s;
    *at++ = '"';
    while (p < sin->s+sin->len) {
		if(*p == '"') {
			*at++ = '"';
		}
        *at++ = *p++;
    }
    *at++ = '"';
    *at = 0;
    sout->len = at - sout->s;
    LM_DBG("escaped string is <%s>\n", sout->s);

    return 0;
}

int cmp_str(str *s1, str *s2)
{
	int ret = 0;
	int len = 0;
	if(s1->len==0 && s2->len==0)
		return 0;
	if(s1->len==0)
		return -1;
	if(s2->len==0)
		return 1;
	len = (s1->len<s2->len)?s1->len:s2->len;
	ret = strncmp(s1->s, s2->s, len);
	if(ret==0)
	{
		if(s1->len==s2->len)
			return 0;
		if(s1->len<s2->len)
			return -1;
		return 1;
	}
	return ret;
}

int cmpi_str(str *s1, str *s2)
{
	int ret = 0;
	int len = 0;
	if(s1->len==0 && s2->len==0)
		return 0;
	if(s1->len==0)
		return -1;
	if(s2->len==0)
		return 1;
	len = (s1->len<s2->len)?s1->len:s2->len;
	ret = strncasecmp(s1->s, s2->s, len);
	if(ret==0)
	{
		if(s1->len==s2->len)
			return 0;
		if(s1->len<s2->len)
			return -1;
		return 1;
	}
	return ret;
}

int cmp_hdrname_str(str *s1, str *s2)
{
	/* todo: parse hdr name and compare with short/long alternative */
	return cmpi_str(s1, s2);
}

int cmp_hdrname_strzn(str *s1, char *s2, size_t n)
{
	str s;
	s.s = s2;
	s.len = n;
	return cmpi_str(s1, &s);
}

int cmp_str_params(str *s1, str *s2)
{
	param_t* pl1 = NULL;
	param_hooks_t phooks1;
	param_t *pit1=NULL;
	param_t* pl2 = NULL;
	param_hooks_t phooks2;
	param_t *pit2=NULL;

	if (parse_params(s1, CLASS_ANY, &phooks1, &pl1)<0)
		return -1;
	if (parse_params(s2, CLASS_ANY, &phooks2, &pl2)<0)
		return -1;
	for (pit1 = pl1; pit1; pit1=pit1->next)
	{
		for (pit2 = pl2; pit2; pit2=pit2->next)
		{
			if (pit1->name.len==pit2->name.len
				&& strncasecmp(pit1->name.s, pit2->name.s, pit2->name.len)==0)
			{
				if(pit1->body.len!=pit2->body.len
						|| strncasecmp(pit1->body.s, pit2->body.s,
							pit2->body.len)!=0)
					return 1;
			}
		}
	}
	return 0;
}

/**
 * Compare SIP URI as per RFC3261, 19.1.4
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_uri(struct sip_uri *uri1, struct sip_uri *uri2)
{
	if(uri1->type!=uri2->type)
		return 1;
	/* quick check for length */
	if(uri1->user.len!=uri2->user.len
			|| uri1->host.len!=uri2->host.len
			|| uri1->port.len!=uri2->port.len
			|| uri1->passwd.len!=uri2->passwd.len)
		return 1;
	if(cmp_str(&uri1->user, &uri2->user)!=0)
		return 1;
	if(cmp_str(&uri1->port, &uri2->port)!=0)
		return 1;
	if(cmp_str(&uri1->passwd, &uri2->passwd)!=0)
		return 1;
	if(cmpi_str(&uri1->host, &uri2->host)!=0)
		return 1;
	/* if no params, we are done */
	if(uri1->params.len==0 && uri2->params.len==0)
		return 0;
	if(uri1->params.len==0)
	{
		if(uri2->user_param.len!=0)
			return 1;
		if(uri2->ttl.len!=0)
			return 1;
		if(uri2->method.len!=0)
			return 1;
		if(uri2->maddr.len!=0)
			return 1;
	}
	if(uri2->params.len==0)
	{
		if(uri1->user_param.len!=0)
			return 1;
		if(uri1->ttl.len!=0)
			return 1;
		if(uri1->method.len!=0)
			return 1;
		if(uri1->maddr.len!=0)
			return 1;
	}
	return cmp_str_params(&uri1->params, &uri2->params);
}

/**
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_uri_str(str *s1, str *s2)
{
	struct sip_uri uri1;
	struct sip_uri uri2;

	/* todo: parse uri and compare the parts */
	if(parse_uri(s1->s, s1->len, &uri1)!=0)
		return -1;
	if(parse_uri(s2->s, s2->len, &uri2)!=0)
		return -1;
	return cmp_uri(&uri1, &uri2);
}

/**
 * Compare SIP AoR
 * - match user, host and port (if port missing, assume 5060)
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_aor(struct sip_uri *uri1, struct sip_uri *uri2)
{
	/* quick check for length */
	if(uri1->user.len!=uri2->user.len
			|| uri1->host.len!=uri2->host.len)
		return 1;
	if(cmp_str(&uri1->user, &uri2->user)!=0)
		return 1;
	if(cmp_str(&uri1->port, &uri2->port)!=0)
	{
		if(uri1->port.len==0 && uri2->port_no!=5060)
			return 1;
		if(uri2->port.len==0 && uri1->port_no!=5060)
			return 1;
	}
	if(cmpi_str(&uri1->host, &uri2->host)!=0)
		return 1;
	return 0;
}

/**
 * return:
 *	- 0: match
 *	- >0: no match
 *	- <0: error
 */
int cmp_aor_str(str *s1, str *s2)
{
	struct sip_uri uri1;
	struct sip_uri uri2;

	/* todo: parse uri and compare the parts */
	if(parse_uri(s1->s, s1->len, &uri1)!=0)
		return -1;
	if(parse_uri(s2->s, s2->len, &uri2)!=0)
		return -1;
	return cmp_aor(&uri1, &uri2);
}

/*! \brief Replace in replacement tokens \\d with substrings of string pointed by
 * pmatch.
 */
int replace(regmatch_t* pmatch, char* string, char* replacement, str* result)
{
	int len, i, j, digit, size;

	len = strlen(replacement);
	j = 0;

	for (i = 0; i < len; i++) {
		if (replacement[i] == '\\') {
			if (i < len - 1) {
				if (isdigit((unsigned char)replacement[i+1])) {
					digit = replacement[i+1] - '0';
					if (pmatch[digit].rm_so != -1) {
						size = pmatch[digit].rm_eo - pmatch[digit].rm_so;
						if (j + size < result->len) {
							memcpy(&(result->s[j]), string+pmatch[digit].rm_so, size);
							j = j + size;
						} else {
							return -1;
						}
					} else {
						return -2;
					}
					i = i + 1;
					continue;
				} else {
					i = i + 1;
				}
			} else {
				return -3;
			}
		}
		if (j + 1 < result->len) {
			result->s[j] = replacement[i];
			j = j + 1;
		} else {
			return -4;
		}
	}
	result->len = j;
	return 1;
}


#define SR_RE_MAX_MATCH 6

/*! \brief Match pattern against string and store result in pmatch */
int reg_match(char *pattern, char *string, regmatch_t *pmatch)
{
	regex_t preg;

	if (regcomp(&preg, pattern, REG_EXTENDED | REG_NEWLINE)) {
		return -1;
	}
	if (preg.re_nsub > SR_RE_MAX_MATCH) {
		regfree(&preg);
		return -2;
	}
	if (regexec(&preg, string, SR_RE_MAX_MATCH, pmatch, 0)) {
		regfree(&preg);
		return -3;
	}
	regfree(&preg);
	return 0;
}


/*! \brief Match pattern against string and, if match succeeds, and replace string
 * with replacement substituting tokens \\d with matched substrings.
 */
int reg_replace(char *pattern, char *replacement, char *string, str *result)
{
	regmatch_t pmatch[SR_RE_MAX_MATCH];

	LM_DBG("pattern: '%s', replacement: '%s', string: '%s'\n",
	    pattern, replacement, string);

	if (reg_match(pattern, string, &(pmatch[0]))) {
		return -1;
	}

	return replace(&pmatch[0], string, replacement, result);

}
