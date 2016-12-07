/*
 * Copyright (C) 2008 iptelorg GmbH
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
 *
 */

/*! \file
 * \brief Parser :: Body handling
 *
 * \ingroup parser
 */


#include "../trim.h"
#include "parser_f.h"
#include "parse_content.h"
#include "parse_param.h"
#include "keys.h"
#include "parse_body.h"

/*! \brief returns the value of boundary parameter from the Contect-Type HF */
static inline int get_boundary_param(struct sip_msg *msg, str *boundary)
{
	str	s;
	char	*c;
	param_t	*p, *list;

#define is_boundary(c) \
	(((c)[0] == 'b' || (c)[0] == 'B') && \
	((c)[1] == 'o' || (c)[1] == 'O') && \
	((c)[2] == 'u' || (c)[2] == 'U') && \
	((c)[3] == 'n' || (c)[3] == 'N') && \
	((c)[4] == 'd' || (c)[4] == 'D') && \
	((c)[5] == 'a' || (c)[5] == 'A') && \
	((c)[6] == 'r' || (c)[6] == 'R') && \
	((c)[7] == 'y' || (c)[7] == 'Y'))

#define boundary_param_len (sizeof("boundary")-1)

	/* get the pointer to the beginning of the parameter list */
	s.s = msg->content_type->body.s;
	s.len = msg->content_type->body.len;
	c = find_not_quoted(&s, ';');
	if (!c)
		return -1;
	c++;
	s.len = s.len - (c - s.s);
	s.s = c;
	trim_leading(&s);

	if (s.len <= 0)
		return -1;

	/* parse the parameter list, and search for boundary */
	if (parse_params(&s, CLASS_ANY, NULL, &list)<0)
		return -1;

	boundary->s = NULL;
	for (p = list; p; p = p->next)
		if ((p->name.len == boundary_param_len) &&
			is_boundary(p->name.s)
		) {
			boundary->s = p->body.s;
			boundary->len = p->body.len;
			break;
		}
	free_params(list);
	if (!boundary->s || !boundary->len)
		return -1;

	DBG("boundary is \"%.*s\"\n",
		boundary->len, boundary->s);
	return 0;
}

/*! \brief search the next boundary in the buffer */
static inline char *search_boundary(char *buf, char *buf_end, str *boundary)
{
	char *c;

	c = buf;
	while (c + 2 /* -- */ + boundary->len < buf_end) {
		if ((*c == '-') && (*(c+1) == '-') &&
			(memcmp(c+2, boundary->s, boundary->len) == 0)
		)
			return c; /* boundary found */

		/* go to the next line */
		while ((c < buf_end) && (*c != '\n')) c++;
		c++;
	}
	return NULL;
}

/*! \brief extract the body of a part from a multipart SIP msg body */
inline static char *get_multipart_body(char *buf,
					char *buf_end,
					str *boundary,
					int *len)
{
	char *beg, *end;

	if (buf >= buf_end)
		goto error;

	beg = buf;
	while ((*beg != '\r') && (*beg != '\n')) {
		while ((beg < buf_end) && (*beg != '\n'))
			beg++;
		beg++;
		if (beg >= buf_end)
			goto error;
	}
	/* CRLF delimeter found, the body begins right after it */
	while ((beg < buf_end) && (*beg != '\n'))
		beg++;
	beg++;
	if (beg >= buf_end)
		goto error;

	if (!(end = search_boundary(beg, buf_end, boundary)))
		goto error;

	/* CRLF preceding the boundary belongs to the boundary
	and not to the body */
	if (*(end-1) == '\n') end--;
	if (*(end-1) == '\r') end--;

	if (end < beg)
		goto error;

	*len = end-beg;
	return beg;
error:
	ERR("failed to extract the body from the multipart mime type\n");
	return NULL;
}


/*! \brief macros from parse_hname2.c */
#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))

#define LOWER_DWORD(d) ((d) | 0x20202020)

/*! \brief Returns the pointer within the msg body to the given type/subtype,
 * and sets the length of the body part.
 * The result can be the whole msg body, or a part of a multipart body.
 */
char *get_body_part(	struct sip_msg *msg,
			unsigned short type, unsigned short subtype,
			int *len)
{
	int	mime;
	unsigned int	umime;
	char	*c, *c2, *buf_end;
	str	boundary;

#define content_type_len \
	(sizeof("Content-Type") - 1)

	if ((mime = parse_content_type_hdr(msg)) <= 0)
		return NULL;

	if (mime == ((type<<16)|subtype)) {
		/* content-type is type/subtype */
		c = get_body(msg);
		if (c)
			*len = msg->buf+msg->len - c;
		return c;

	} else if ((mime>>16) == TYPE_MULTIPART) {
		/* type is multipart/something, search for type/subtype part */

		if (get_boundary_param(msg, &boundary)) {
			ERR("failed to get boundary parameter\n");
			return NULL;
		}
		if (!(c = get_body(msg)))
			return NULL;
		buf_end = msg->buf+msg->len;

		/* check all the body parts delimated by the boundary value,
		and search for the Content-Type HF with the given 
		type/subtype */
next_part:
		while ((c = search_boundary(c, buf_end, &boundary))) {
			/* skip boundary */
			c += 2 + boundary.len;

			if ((c+2 > buf_end) ||
				((*c == '-') && (*(c+1) == '-'))
			)
				/* end boundary, no more body part
				will follow */
				return NULL;

			/* go to the next line */
			while ((c < buf_end) && (*c != '\n')) c++;
			c++;
			if (c >= buf_end)
				return NULL;

			/* try to find the content-type header */
			while ((*c != '\r') && (*c != '\n')) {
				if (c + content_type_len >= buf_end)
					return NULL;

				if ((LOWER_DWORD(READ(c)) == _cont_) &&
					(LOWER_DWORD(READ(c+4)) == _ent__) &&
					(LOWER_DWORD(READ(c+8)) == _type_)
				) {
					/* Content-Type HF found */
					c += content_type_len;
					while ((c < buf_end) &&
						((*c == ' ') || (*c == '\t'))
					)
						c++;

					if (c + 1 /* : */ >= buf_end)
						return NULL;

					if (*c != ':')
						/* not realy a Content-Type HF */
						goto next_hf;
					c++;

					/* search the end of the header body,
					decode_mime_type() needs it */
					c2 = c;
					while (((c2 < buf_end) && (*c2 != '\n')) ||
						((c2+1 < buf_end) && (*c2 == '\n') &&
							((*(c2+1) == ' ') || (*(c2+1) == '\t')))
					)
						c2++;

					if (c2 >= buf_end)
						return NULL;
					if (*(c2-1) == '\r') c2--;

					if (!decode_mime_type(c, c2 , &umime)) {
						ERR("failed to decode the mime type\n");
						return NULL;
					}

					/* c2 points to the CRLF at the end of the line,
					move the pointer to the beginning of the next line */
					c = c2;
					if ((c < buf_end) && (*c == '\r')) c++;
					if ((c < buf_end) && (*c == '\n')) c++;

					if (umime != ((type<<16)|subtype)) {
						/* this is not the part we are looking for */
						goto next_part;
					}

					/* the requested type/subtype is found! */
					return get_multipart_body(c,
							buf_end,
							&boundary,
							len);
				}
next_hf:
				/* go to the next line */
				while ((c < buf_end) && (*c != '\n')) c++;
				c++;
			}
			/* CRLF delimeter reached,
			no Content-Type HF was found */
		}
	}
	return NULL;
}


/**
 * trim_leading_hts
 *
 * trim leading all spaces ' ' and horizontal tabs '\t' characters.
 *   - buffer, pointer to the beginning of the buffer.
 *   - end_buffer, pointer to the end of the buffer.
 * returns
 *   - pointer to the first non-match character if success.
 *   - pointer to NULL if the end_buffer is reached.
 */
char *trim_leading_hts (char *buffer, char *end_buffer)
{
	char *cpy_buffer = buffer;
	while ((cpy_buffer < end_buffer) &&
			((*cpy_buffer == ' ') || (*cpy_buffer == '\t'))) {
		cpy_buffer++;
	}

	return ((cpy_buffer < end_buffer) ? cpy_buffer : NULL);
}


/**
 * trim_leading_e_r
 *
 * trim leading characters until get a '\r'.
 *   - buffer, pointer to the beginning of the buffer.
 *   - end_buffer, pointer to the end of the buffer.
 *
 * returns
 *   - pointer to the first '\r' character if success.
 *   - pointer to NULL if the end_buffer is reached.
 */
char *trim_leading_e_r (char *buffer, char *end_buffer)
{
	char *cpy_buffer = buffer;
	while ((cpy_buffer < end_buffer) && (*cpy_buffer != '\r')) {
		cpy_buffer++;
	}
	return ((cpy_buffer < end_buffer) ? cpy_buffer : NULL);
}


/**
 * part_multipart_headers_cmp
 * trim leading characters until get a '\r'.
 * receives
 *   - buffer, pointer to the beginning of the headers in a part of the multipart body.
 *   - end_buffer, pointer to the end of the headers in the multipart body.
 *   - content type/ content subtype.
 *         if (type == 0 / subtype == 0): Content-Type: disabled in the search.
 *   - content id.
 *         if (id == NULL): Content-ID: disabled in the search.
 *   - content length.
 *         if (length == NULL) Content-Length: disabled in the search.
 *
 * returns
 *   - true, if the part of the multipart body has :
 *            -- Content-Type   that matches content_type / content_subtype. (if Content-Type enabled) &&
 *            -- Content-ID     that matches content_id. (if Content-ID enabled) &&
 *            -- Content-Length that matches content_length. (if Content-Length enabled)
 *   - false, if any of them doesnt match.
 */
int part_multipart_headers_cmp (char *buffer,
				char *end_buffer,
				unsigned short content_type,
				unsigned short content_subtype,
				char *content_id,
				char *content_length)
{
	int error = 0;
	char *error_msg = NULL;

	char *cpy_c = NULL;
	char *cpy_d = NULL;

	char *value_ini = NULL;
	char *value_fin = NULL;
	unsigned int umime;

	int found = 0;
	int found_content_type   = 0;
	int found_content_id     = 0;
	int found_content_length = 0;

	if ((buffer == NULL) || (end_buffer == NULL)) {
		error = -1;
		error_msg = "buffer and/or end_buffer are NULL";
	} else {
		cpy_c = buffer;
		cpy_d = end_buffer;

		if ((content_type == 0) && (content_subtype == 0)) {
			found_content_type   = 1;
		}
		if (content_id == NULL) {
			found_content_id = 1;
		}
		if (content_length == NULL) {
			found_content_length = 1;
		}

		found = found_content_type * found_content_id * found_content_length;
		while ((!found) && (!error) && (cpy_c < cpy_d)) {
			if ((cpy_c + 8) < cpy_d) {
				if ( (LOWER_DWORD(READ(cpy_c)) == _cont_)
						&& (LOWER_DWORD(READ(cpy_c + 4)) == _ent__) ) {
					cpy_c += 8;
					if ( (!found_content_type)
							&& ((cpy_c + 5) < cpy_d)
							&& ((*(cpy_c + 0) == 't') || (*(cpy_c + 0) == 'T'))
							&& ((*(cpy_c + 1) == 'y') || (*(cpy_c + 1) == 'Y'))
							&& ((*(cpy_c + 2) == 'p') || (*(cpy_c + 2) == 'P'))
							&& ((*(cpy_c + 3) == 'e') || (*(cpy_c + 3) == 'E'))
							&& (*(cpy_c + 4) == ':') ) {
						cpy_c += 5;
						/* value_ has the content of the header */
						value_ini = trim_leading_hts(cpy_c, cpy_d);
						value_fin = trim_leading_e_r(cpy_c, cpy_d);
						if ((value_ini != NULL) && (value_fin != NULL)) {
							cpy_c = value_fin;
							if (decode_mime_type(value_ini, value_fin, &umime)) {
								if (umime == ((content_type<<16)|content_subtype)) {
									found_content_type = 1;
								} else {
									error = -2;
									error_msg = "MIME types mismatch";
								}
							} else {
								error = -3;
								error_msg = "Failed to decode MIME type";
							}
						} else {
							error = -4;
							error_msg = "Failed to perform trim_leading_hts || trim_leading_e_r";
						}
					} else if( (!found_content_id) && ((cpy_c + 3) < cpy_d)
							&& ((*(cpy_c + 0) == 'i') || (*(cpy_c + 0) == 'I'))
							&& ((*(cpy_c + 1) == 'd') || (*(cpy_c + 1) == 'D'))
							&& (*(cpy_c + 2) == ':') ) {
						cpy_c += 3;
						/* value_ has the content of the header */
						value_ini = trim_leading_hts(cpy_c, cpy_d);
						value_fin = trim_leading_e_r(cpy_c, cpy_d);
						if ((value_ini != NULL) && (value_fin != NULL)) {
							cpy_c = value_fin;
							if (strncmp(content_id, value_ini, value_fin-value_ini) == 0) {
								found_content_id = 1;
							} else {
								error = -5;
								error_msg = "Content-ID mismatch";
							}
						} else {
							error = -6;
							error_msg = "Failed to perform trim_leading_hts || trim_leading_e_r";
						}
					} else if( (!found_content_length) && ((cpy_c + 7) < cpy_d)
							&& ((*(cpy_c + 0) == 'l') || (*(cpy_c + 0) == 'L'))
							&& ((*(cpy_c + 1) == 'e') || (*(cpy_c + 1) == 'E'))
							&& ((*(cpy_c + 2) == 'n') || (*(cpy_c + 2) == 'N'))
							&& ((*(cpy_c + 3) == 'g') || (*(cpy_c + 3) == 'G'))
							&& ((*(cpy_c + 4) == 't') || (*(cpy_c + 4) == 'T'))
							&& ((*(cpy_c + 5) == 'h') || (*(cpy_c + 5) == 'H'))
							&& (*(cpy_c + 6) == ':') ) {
						cpy_c += 7;
						/* value_ has the content of the header */
						value_ini = trim_leading_hts(cpy_c, cpy_d);
						value_fin = trim_leading_e_r(cpy_c, cpy_d);
						if ((value_ini != NULL) && (value_fin != NULL)) {
							cpy_c = value_fin;
							if (strncmp(content_length, value_ini, value_fin-value_ini) == 0) {
								found_content_length = 1;
							} else {
								error = -7;
								error_msg = "Content-Length mismatch";
							}
						} else {
							error = -8;
							error_msg = "Failed to perform trim_leading_hts || trim_leading_e_r";
						}
					} else {
						/* Next characters dont match "Type:" or "ID:" or "Length:" OR
					     * header already parsed (maybe duplicates?) and founded OR
					     * header initially set as disabled and it doesnt need to be treated.
					     * This is NOT an error. */
						;
					}
				} else {
					/* First 8 characters dont match "Content-"
					 * This is NOT an error. */
					;
				}
			} else {
				error = -9;
				error_msg = "We reached the end of the buffer";
			}
			found = found_content_type * found_content_id * found_content_length;
			if ((!found) && (!error)) {
				value_fin = trim_leading_e_r(cpy_c, cpy_d);
				if (value_fin != NULL) {
					cpy_c = value_fin;
					if ((cpy_c + 1) < cpy_d) {
						if ((*cpy_c == '\r') && (*(cpy_c + 1) == '\n')) {
							cpy_c++;
							cpy_c++;
						} else {
							error = -10;
							error_msg = "Each line must end with a \r\n";
						}
					} else {
						error = -11;
						error_msg = "We reached the end of the buffer";
					}
				} else {
					error = -12;
					error_msg = "Failed to perform trim_leading_e_r";
				}
			}
		} /* End main while loop */
	}

	if (error < 0) {
		LM_ERR("part_multipart_headers_cmp. error. \"%i\". \"%s\".\n", error, error_msg);
		return 0;
	} else {
		return found;
	}
}

/**
 * get_body_part_by_filter
 *
 * Filters the multipart part from a given SIP message which matches the
 * Content-Type && || Content-ID  && || Content-Length
 * receives
 *   - SIP message
 *   - pointer to the beginning of the headers in a part of the multipart body.
 *   - pointer to the end of the headers in the multipart body.
 *   - content type/ content subtype.
 *         if (type == 0 / subtype == 0): Content-Type: disabled in the search.
 *   - content id.
 *         if (id == NULL): Content-ID: disabled in the search.
 *   - content length.
 *         if (length == NULL) Content-Length: disabled in the search.
 *   - len. Length of the multipart message returned.
 *
 * returns
 *   - pointer to the multipart if success.
 *   - NULL, if none of the multiparts match.
 */
char *get_body_part_by_filter(struct sip_msg *msg,
		     unsigned short content_type,
		     unsigned short content_subtype,
		     char *content_id,
		     char *content_length,
		     int *len)
{
	int mime;
	char*c, *d, *buf_end;
	str boundary;

	if ((mime = parse_content_type_hdr(msg)) <= 0)
		return NULL;

	if ((mime>>16) == TYPE_MULTIPART) {
		/* type is multipart/something, search for type/subtype part */
		if (get_boundary_param(msg, &boundary)) {
			ERR("failed to get boundary parameter\n");
			return NULL;
		}
		if (!(c = get_body(msg)))
			return NULL;
		buf_end = msg->buf+msg->len;

		while ((c = search_boundary(c, buf_end, &boundary))) {
			/* skip boundary */
			c += 2 + boundary.len;

			if ((c+2 > buf_end) || ((*c == '-') && (*(c+1) == '-')) )
				/* end boundary, no more body part will follow */
				return NULL;

			/* go to the next line */
			while ((c < buf_end) && (*c != '\n')) c++;
			c++;
			if (c >= buf_end)
				return NULL;

			d = get_multipart_body(c, buf_end, &boundary, len);
			if (part_multipart_headers_cmp(c, d, content_type, content_subtype,
						content_id, content_length)) {
				return d;
			}
		}
	}
	return NULL;
}
