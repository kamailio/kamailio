/*
 * 
 * Copyright (c) 2006 SOMA Networks, Inc. <http://www.somanetworks.com/>
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

/*!
 * \file
 * \brief SST parser
 * \ingroup parser
 * \author dhsueh@somanetworks.com
 */

#include "parse_sst.h"

#include "../../error.h"
#include "../../dprint.h"
#include "../../mem/mem.h"


static inline int/*bool*/  is_space( char c ) { return (c == ' ' || c == '\t'); }
static inline int/*bool*/  is_num( char c ) { return (c >= '0' && c <= '9'); }

static inline unsigned  lower_byte( char b ) { return b | 0x20; }
static inline unsigned  lower_4bytes( unsigned d ) { return d | 0x20202020; }
static inline unsigned  lower_3bytes( unsigned d ) { return d |   0x202020; }
static inline unsigned  read_4bytes( char *val ) {
	return (*(val + 0) + (*(val + 1) << 8)
		+ (*(val + 2) << 16) + (*(val + 3) << 24));
}
static inline unsigned  read_3bytes( char *val ) {
	return (*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16));
}

/* compile-time constants if called with constants */
#define  MAKE_4BYTES( a, b, c, d ) \
	( ((a)&0xFF) | (((b)&0xFF)<<8) | (((c)&0xFF)<<16) | (((d)&0xFF)<<24) )
#define  MAKE_3BYTES( a, b, c ) \
	( ((a)&0xFF) | (((b)&0xFF)<<8) | (((c)&0xFF)<<16) )


struct session_expires *
malloc_session_expires( void )
{
	struct session_expires *se = (struct session_expires *)
		pkg_malloc( sizeof(struct session_expires) );
	if ( se )
		memset( se, 0, sizeof(struct session_expires) );
	return se;
}

/**
 * wrapper to free the content of parsed session-expires header
 */
void hf_free_session_expires(void *parsed)
{
	struct session_expires *se;
	se = (struct session_expires*)parsed;
	free_session_expires(se);
}


void
free_session_expires( struct session_expires *se )
{
	if ( se )
		pkg_free( se );
}


enum parse_sst_result
parse_session_expires_body( struct hdr_field *hf )
{
	register char *p = hf->body.s;
	int pos = 0;
	int len = hf->body.len;
	char *q;
	struct session_expires se = { 0, 0, sst_refresher_unspecified };
	unsigned tok;

	if ( !p || len <= 0 ) {
		LM_ERR(" no body for header field\n" );
		return parse_sst_header_not_found;
	}

	/* skip whitespace */
	for ( ; pos < len && is_space(*p); ++pos, ++p )
		/*nothing*/;

	/* collect a number */
	for ( q = p; pos < len && is_num(*q); ++pos, ++q )
		se.interval = se.interval*10/*radix*/ + (*q - '0');

	if ( q == p ) /*nothing parsed */ {
		LM_ERR(" no expiry interval\n" );
		return parse_sst_no_value;
	}
	p = q;

	/* continue on with params */
	while ( pos < len ) {

		if ( *p == ';' ) {
			++p; ++pos;

			if ( pos + 4 < len ) {
				switch ( lower_4bytes(read_4bytes(p)) ) {
					case /*refr*/MAKE_4BYTES('r','e','f','r'):
						if ( pos + 9 <= len
							 && lower_4bytes(read_4bytes(p+4))
								== /*eshe*/MAKE_4BYTES('e','s','h','e')
							 && lower_byte(*(p+8)) == 'r'
							 && *(p+9) == '=' ) {
							tok = lower_3bytes( read_3bytes(p+10) );
							if ( tok == MAKE_3BYTES('u','a','c') ) {
								se.refresher = sst_refresher_uac;
								p += 13; pos += 13;
							}
							else if ( tok == MAKE_3BYTES('u','a','s') ) {
								se.refresher = sst_refresher_uas;
								p += 13; pos += 13;
							}
							else /* unrecognized refresher-param */ {
								LM_ERR(" unrecognized refresher\n" );
								return parse_sst_parse_error;
							}
						}
						else /* not "esher=" */ {
							/* there are no other se-params 
							   that start with "refr" */
							for ( ; pos < len && *p != ';'; ++pos, ++p )
								/*skip to ';'*/;
						}
						break;
					default:
						/* unrecognized se-param */
						for ( ; pos < len && *p != ';'; ++pos, ++p )
							/*skip to ';'*/;
						break;
				} /*switch*/
			} /* exist 4 bytes to check */
			else /* less than 4 bytes left */ {
				/* not enough text left for any of the recognized se-params */
				/* no other recognized se-param */
				for ( ; pos < len && *p != ';'; ++pos, ++p ) /*skip to ';'*/;
			}
		}
		else /* not ';' */ {
			LM_ERR("no semicolon separating se-params\n");
			return parse_sst_parse_error;
		} /* if ';' */
	} /* while */

	hf->parsed = malloc_session_expires();
	if ( !hf->parsed ) {
		LM_ERR(" out of pkg memory\n" );
		return parse_sst_out_of_mem;
	}
	se.hfree = hf_free_session_expires;
	*((struct session_expires *)hf->parsed) = se;

	return parse_sst_success;
}


enum parse_sst_result
parse_session_expires( struct sip_msg *msg, struct session_expires *se )
{
	enum parse_sst_result result;

	if ( msg->session_expires ) {
		if ( msg->session_expires->parsed == 0
			 && (result = parse_session_expires_body(msg->session_expires))
				!= parse_sst_success ) {
			return result;
		}
		if ( se ) {
			*se = *((struct session_expires *)msg->session_expires->parsed);
		}
		return parse_sst_success;
	}
	else {
		return parse_sst_header_not_found;
	}
}


enum parse_sst_result
parse_min_se_body( struct hdr_field *hf )
{
	int len = hf->body.len;
	char *p = hf->body.s;
	int pos = 0;
	unsigned int interval = 0;

	/* skip whitespace */
	for ( ; pos < len && is_space(*p); ++pos, ++p )
		/*nothing*/;
	if ( pos == len )
		return parse_sst_no_value;
	/* collect a number */
	for ( ; pos < len && is_num(*p); ++pos, ++p )
		interval = interval*10/*radix*/ + (*p - '0');
	/* skip whitespace */
	for ( ; pos < len && is_space(*p); ++pos, ++p )
		/*nothing*/;
	if ( pos != len ) /* shouldn't be any more junk */
		return parse_sst_parse_error;
	hf->parsed=(void*)(long)interval;
	return parse_sst_success;
}


enum parse_sst_result
parse_min_se( struct sip_msg *msg, unsigned int *min_se )
{
	enum parse_sst_result result;

	if ( msg->min_se ) {
		if ( msg->min_se->parsed == 0
			 && (result = parse_min_se_body(msg->min_se))
				!= parse_sst_success ) {
			return result;
		}
		if ( min_se ) {
			*min_se = (unsigned int)(long)msg->min_se->parsed;
		}
		return parse_sst_success;
	}
	else {
		return parse_sst_header_not_found;
	}
}
