/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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

#include "mark.h"
#include "../../str.h"
#include "../../data_lump.h"

const str psu_hdr_s = str_init("P-Served-User: <%.*s>;sescase=%.*s;regstate=%.*s\r\n");
const str sescase_orig = str_init("orig");
const str sescase_term = str_init("term");
const str regstate_reg = str_init("reg");
const str regstate_unreg = str_init("unreg");

extern int add_p_served_user;

/** base16 char constants */
char *hexchars = "0123456789abcdef";
/**
 * Converts a binary encoded value to its base16 representation.
 * @param from - buffer containing the input data
 * @param len - the size of from
 * @param to - the output buffer  !!! must have at least len*2 allocated memory 
 * @returns the written length 
 */
int bin_to_base16(char *from, int len, char *to) {
	int i, j;
	for (i = 0, j = 0; i < len; i++, j += 2) {
		to[j] = hexchars[(((unsigned char) from[i]) >> 4) & 0x0F];
		to[j + 1] = hexchars[(((unsigned char) from[i])) & 0x0F];
	}
	return 2 * len;
}

/** char to hex convertor */
#define HEX_VAL(c)	( (c<='9')?(c-'0'):(c-'A'+10) )

/**
 *	Retrieves the mark from message.
 *		- the marking should be in a header like described before
 *	@param msg - SIP mesage to mark
 *  @param mark - mark to load into
 *	@returns 1 if found, 0 if not
 */
int isc_mark_get_from_msg(struct sip_msg *msg, isc_mark *mark) {
	struct hdr_field *hdr;
	rr_t *rr;
	str x;
	LM_DBG("isc_mark_get_from_msg: Trying to get the mark from the message \n");

	memset(mark, 0, sizeof(isc_mark));

	parse_headers(msg, HDR_EOH_F, 0);
	hdr = msg->headers;
	while (hdr) {
		if (hdr->type == HDR_ROUTE_T) {
			if (!hdr->parsed) {
				if (parse_rr(hdr) < 0) {
					LM_ERR("isc_mark_get_from_msg: Error while parsing Route HF\n");
					hdr = hdr->next;
					continue;
				}
			}
			rr = (rr_t*) hdr->parsed;
			while (rr) {
				x = rr->nameaddr.uri;
				if (x.len >= ISC_MARK_USERNAME_LEN + 1 + isc_my_uri.len
						&& strncasecmp(x.s, ISC_MARK_USERNAME,
								ISC_MARK_USERNAME_LEN) == 0
						&& strncasecmp(x.s + ISC_MARK_USERNAME_LEN + 1,
								isc_my_uri.s, isc_my_uri.len) == 0) {
					LM_DBG("isc_mark_get_from_msg: Found <%.*s>\n",	x.len, x.s);
					isc_mark_get(x, mark);
					return 1;
				}
				rr = rr->next;
			}
		}
		hdr = hdr->next;
	}
	return 0;
}

/**
 * Load the mark from a string.
 * @param x - string with the mark, as found in the Route header
 * @param mark - mark to load into
 */
void isc_mark_get(str x, isc_mark *mark) {
	int i, j, k;
	str aor_hex = { 0, 0 };
	if (mark->aor.s)
		pkg_free(mark->aor.s);
	mark->aor = aor_hex;
	for (i = 0; i < x.len && x.s[i] != ';'; i++)
		;
	while (i < x.len) {
		if (x.s[i + 1] == '=') {
			k = 0;
			for (j = i + 2; j < x.len && x.s[j] != ';'; j++)
				k = k * 10 + (x.s[j] - '0');
			switch (x.s[i]) {
			case 's':
				mark->skip = k;
				break;
			case 'h':
				mark->handling = k;
				break;
			case 'd':
				mark->direction = k;
				break;
			case 'a':
				aor_hex.s = x.s + i + 2;
				aor_hex.len = 0;
				for (j = i + 2; j < x.len && x.s[j] != ';'; j++)
					aor_hex.len++;
				mark->aor.len = aor_hex.len / 2;
				mark->aor.s = pkg_malloc(mark->aor.len);
				if (!mark->aor.s) {
					LM_ERR("isc_mark_get: Error allocating %d bytes\n",	mark->aor.len);
					mark->aor.len = 0;
				} else {
					mark->aor.len = base16_to_bin(aor_hex.s, aor_hex.len, mark->aor.s);
				}
				break;
			default:
				LM_ERR("isc_mark_get: unknown parameter found: %c !\n", x.s[i]);
			}
			i = j + 1;
		} else
			i++;
	}
}

/** from base16 char to int */
#define HEX_DIGIT(x) \
	((x>='0'&&x<='9')?x-'0':((x>='a'&&x<='f')?x-'a'+10:((x>='A'&&x<='F')?x-'A'+10:0)))
/**
 * Converts a hex encoded value to its binary value
 * @param from - buffer containing the input data
 * @param len - the size of from
 * @param to - the output buffer  !!! must have at least len/2 allocated memory 
 * @returns the written length 
 */
int base16_to_bin(char *from, int len, char *to) {
	int i, j;
	for (i = 0, j = 0; j < len; i++, j += 2) {
		to[i] = (unsigned char) (HEX_DIGIT(from[j]) << 4 | HEX_DIGIT(from[j+1]));
	}
	return i;
}

/**
 *	Deletes the previous marking attempts (lumps).
 *
 *	@param msg - SIP mesage to mark
 *	@returns 1 on success
 */
inline int isc_mark_drop_route(struct sip_msg *msg) {
	struct lump* lmp, *tmp;

	parse_headers(msg, HDR_EOH_F, 0);

	anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);

	LM_DBG("ifc_mark_drop_route: Start --------- \n");
	lmp = msg->add_rm;
	while (lmp) {
		tmp = lmp->before;
		if (tmp && tmp->op == LUMP_ADD && tmp->u.value
				&& strstr(tmp->u.value, ISC_MARK_USERNAME)) {
			LM_DBG("ifc_mark_drop_route: Found lump %s ... dropping\n", tmp->u.value);
			//tmp->op=LUMP_NOP;			
			tmp->len = 0;
			/*lmp->before = tmp->before;
			 free_lump(tmp);	*/
		}
		lmp = lmp->next;
	}
	LM_DBG("ifc_mark_drop_route: ---------- End \n");

	return 1;
}

/**
 *	Mark the message with the given mark.
 *		- old marking attempts are deleted
 *		- marking is performed by inserting the following header
 *	@param msg - SIP mesage to mark
 *	@param match - the current IFC match
 *	@param mark - pointer to the mark
 *	@returns 1 on success or 0 on failure
 */
int isc_mark_set(struct sip_msg *msg, isc_match *match, isc_mark *mark) {
	str route = { 0, 0 };
	str as = { 0, 0 };
	char chr_mark[256];
	char aor_hex[256];
	int len;

	/* Drop all the old Header Lump "Route: <as>, <my>" */
	isc_mark_drop_route(msg);

	len = bin_to_base16(mark->aor.s, mark->aor.len, aor_hex);
	/* Create the Marking */
	sprintf(chr_mark, "%s@%.*s;lr;s=%d;h=%d;d=%d;a=%.*s", ISC_MARK_USERNAME,
			isc_my_uri.len, isc_my_uri.s, mark->skip, mark->handling,
			mark->direction, len, aor_hex);
	/* Add it in a lump */
	route.s = chr_mark;
	route.len = strlen(chr_mark);
	if (match)
		as = match->server_name;
	isc_mark_write_route(msg, &as, &route);
	if (add_p_served_user) {
	    isc_mark_write_psu(msg, mark);
	}
	LM_DBG("isc_mark_set: NEW mark <%s>\n", chr_mark);

	return 1;
}

/**
 *	Inserts the Route header for marking, before first header.
 * - the marking will be in a header like below
 * - if the "as" parameter is empty: \code Route: <[iscmark]> \endcode
 * - else: \code Route: <sip:as@asdomain.net;lr>, <[iscmark]> \endcode
 * 			 
 *
 *	@param msg - SIP mesage to mark
 *	@param as - SIP addres of the application server to forward to
 *	@param iscmark - the mark to write
 *	@returns 1 on success, else 0
 */
inline int isc_mark_write_route(struct sip_msg *msg, str *as, str *iscmark) {
	struct hdr_field *first;
	struct lump* anchor;
	str route;

	parse_headers(msg, HDR_EOH_F, 0);
	first = msg->headers;
	if (as && as->len) {
		route.s = pkg_malloc(21+as->len+iscmark->len);
		sprintf(route.s, "Route: <%.*s;lr>, <%.*s>\r\n", as->len, as->s,
				iscmark->len, iscmark->s);
	} else {
		route.s = pkg_malloc(18+iscmark->len);
		sprintf(route.s, "Route: <%.*s>\r\n", iscmark->len, iscmark->s);
	}

	route.len = strlen(route.s);
	LM_DBG("isc_mark_write_route: <%.*s>\n", route.len, route.s);

	anchor = anchor_lump(msg, first->name.s - msg->buf, 0, HDR_ROUTE_T);
	if (anchor == NULL) {
		LM_ERR("isc_mark_write_route: anchor_lump failed\n");
		return 0;
	}

	if (!insert_new_lump_before(anchor, route.s, route.len, HDR_ROUTE_T)) {
		LM_ERR("isc_mark_write_route: error creating lump for header_mark\n");
	}
	return 1;
}

/**
 *  Inserts the P-Served-User header on a SIP message
 *  as specified in RFC 5502
 *  @param msg - SIP message
 *  @param mark - the mark containing all required information
 *  @returns 1 on success, else 0
 */
int isc_mark_write_psu(struct sip_msg *msg, isc_mark *mark) {
    struct lump *l = msg->add_rm;
    size_t hlen;
    char * hstr = NULL;
    const str *regstate, *sescase;

    switch(mark->direction) {
    case IFC_ORIGINATING_SESSION:
        regstate = &regstate_reg;
        sescase = &sescase_orig;
        break;
    case IFC_TERMINATING_SESSION:
        regstate = &regstate_reg;
        sescase = &sescase_term;
        break;
    case IFC_TERMINATING_UNREGISTERED:
        regstate = &regstate_unreg;
        sescase = &sescase_term;
        break;
    default:
        LM_ERR("isc_mark_write_psu: unknown direction: %d\n", mark->direction);
        return 0;
    }

    hlen = psu_hdr_s.len - /* 3 "%.*s" */ 12 + mark->aor.len + regstate->len + sescase->len + 1;
    hstr = pkg_malloc(hlen);
    if (hstr == NULL) {
        LM_ERR("isc_mark_write_psu: could not allocate %zu bytes\n", hlen);
        return 0;
    }

    int ret = snprintf(hstr, hlen, psu_hdr_s.s,
            mark->aor.len, mark->aor.s,
            sescase->len, sescase->s,
            regstate->len, regstate->s);
    if (ret >= hlen) {
        LM_ERR("isc_mark_write_psu: invalid string buffer size: %zu, required: %d\n", hlen, ret);
        pkg_free(hstr);
        return 0;
    }

    LM_DBG("isc_mark_write_psu: %.*s\n", (int)hlen - 3 /* don't print \r\n\0 */, hstr);
    if (append_new_lump(&l, hstr, hlen - 1, HDR_OTHER_T) == 0) {
        LM_ERR("isc_mark_write_psu: append_new_lump(%p, \"%.*s\\\r\\n\", %zu, 0) failed\n", &l, (int)hlen - 3 /* don't print \r\n\0 */, hstr, hlen - 1);
        pkg_free(hstr);
        return 0;
    }
    /* hstr will be deallocated when msg will be destroyed */
    return 1;
}
