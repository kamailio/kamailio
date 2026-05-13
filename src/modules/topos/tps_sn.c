/**
 * Copyright (C) 2026 Stefan-Cristian Mititelu (net2phone.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief SIP-router topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#include "../../core/parser/parse_uri.h"
#include "../../core/socket_info.h"
#include "../../core/resolve.h"

#include "tps_sn.h"
#include "tps_msg.h"

/**
 * Return 1 if host is an IPv4 or IPv6
 * Return 0 if host is an FQDN
 */
static int tps_host_is_ip(str *host)
{
	return (str2ip(host) != NULL) || (str2ip6(host) != NULL);
}

/**
 * Return the next-write position in td->cbuf and remaining capacity.
 * Lazily initializes td->cp to the start of cbuf on first call.
 * Always sets *buf and *len to valid values.
 */
static inline void tps_data_write_buf(tps_data_t *td, char **buf, int *len)
{
	if(td->cp == NULL) {
		td->cp = td->cbuf;
	}
	*buf = td->cp;
	*len = TPS_DATA_SIZE - (int)(td->cp - td->cbuf);
}

/**
 * Byte-equal two str values (including empty).
 */
static inline int tps_str_eq(const str *a, const str *b)
{
	return a->len == b->len && (a->len == 0 || memcmp(a->s, b->s, a->len) == 0);
}

/**
 * Write `src` into `dst` with the host:port swapped for new_host:new_port.
 * Returns total bytes written after swap
 * Returns -1 on if no space to write those total bytes
 */
static int tps_swap_uri_hostport(const char *src, int src_len,
		const struct sip_uri *puri, const str *new_host, const str *new_port,
		char *dst, int dst_len)
{
	int host_off = (int)(puri->host.s - src);
	int after_port = (puri->port.len > 0)
							 ? (int)(puri->port.s + puri->port.len - src)
							 : (int)(puri->host.s + puri->host.len - src);
	int suffix_len = src_len - after_port;
	int total = host_off + new_host->len + 1 + new_port->len + suffix_len;
	char *p = dst;

	if(total > dst_len) {
		return -1;
	}
	memcpy(p, src, host_off);
	p += host_off;
	memcpy(p, new_host->s, new_host->len);
	p += new_host->len;
	*p++ = ':';
	memcpy(p, new_port->s, new_port->len);
	p += new_port->len;
	if(suffix_len > 0) {
		memcpy(p, src + after_port, suffix_len);
	}
	return total;
}

/**
 * Extract the ;sn= value into out_sn, from a single, parsed, RR entry.
 * Returns 1 if the RR entry has no parseable URI or no ;sn= param.
 * Returns 0 if socket name extracted successfully
 */
static int tps_rr_entry_sn(rr_t *entry, str *out_sn)
{
	struct sip_uri puri;
	str sn_name = str_init("sn");

	out_sn->s = NULL;
	out_sn->len = 0;
	if(entry == NULL
			|| parse_uri(entry->nameaddr.uri.s, entry->nameaddr.uri.len, &puri)
					   < 0) {
		return 1;
	}
	if(tps_get_param_value(&puri.params, &sn_name, out_sn) != 0
			|| out_sn->len <= 0) {
		return 1;
	}
	return 0;
}

/**
 * Detect whether an RR entry would be rebuilt by tps_rebuild_srr_from_sn.
 *
 * Returns 1 if the entry has a ;sn= that resolves to a local socket
 * whose host:port differs from the entry's stored host:port.
 *
 * Returns 0 for any reason that would route the entry through the verbatim-copy
 * path (no ;sn=, unresolvable sockname, host:port already match).
 */
static int tps_rr_entry_needs_rebuild(rr_t *entry)
{
	struct sip_uri puri;
	str sn_name = str_init("sn");
	str sn;
	socket_info_t *si;
	str *host;
	str *port;

	if(entry == NULL
			|| parse_uri(entry->nameaddr.uri.s, entry->nameaddr.uri.len, &puri)
					   < 0) {
		return 0;
	}
	/* FQDN host keep it as is */
	if(!tps_host_is_ip(&puri.host)) {
		return 0;
	}
	if(tps_get_param_value(&puri.params, &sn_name, &sn) != 0 || sn.len <= 0) {
		return 0;
	}
	si = ksr_get_socket_by_name(&sn);
	if(si == NULL) {
		return 0;
	}
	if(si_get_signaling_data(si, &host, &port) < 0 || host == NULL
			|| port == NULL || host->len <= 0 || port->len <= 0) {
		return 0;
	}
	if(tps_str_eq(&puri.host, host) && tps_str_eq(&puri.port, port)) {
		return 0;
	}
	return 1;
}

/**
 * Build a refreshed topos self-contact value (with surrounding <>) into
 * `buf`. Swaps the host:port slice of the stored URI for the local
 * socket's host:port, preserving everything else.
 *
 * A stored host that is a FQDN is left untouched
 *
 * Returns 0 for rebuilt value != stored_scontact.
 * Returns 1 for rebuilt value == stored_scontact (or kept as-is).
 * Returns -1 on error.
 */
static int tps_rebuild_scontact_from_sn(
		str *sn, str *stored_scontact, char *buf, int buf_len, str *out)
{
	socket_info_t *si;
	str *host;
	str *port;
	struct sip_uri puri;
	const char *src;
	int src_len;
	const char *inner;
	int inner_len;
	int n;

	if(sn == NULL || sn->len <= 0 || stored_scontact == NULL
			|| stored_scontact->len <= 0 || buf == NULL || out == NULL) {
		return -1;
	}

	si = ksr_get_socket_by_name(sn);
	if(si == NULL) {
		LM_DBG("no local socket named [%.*s]\n", sn->len, sn->s);
		return -1;
	}

	/* get host/port part corresponding to that socket name */
	if(si_get_signaling_data(si, &host, &port) < 0 || host == NULL
			|| port == NULL || host->len <= 0 || port->len <= 0) {
		return -1;
	}

	/* Stored value is "<sip:[ab]tpsh-UUID@host:port[;params]>".
	 * Strip the wrapping <...> so parse_uri sees only the URI. */
	src = stored_scontact->s;
	src_len = stored_scontact->len;
	inner = src;
	inner_len = src_len;
	if(inner_len >= 2 && inner[0] == '<' && inner[inner_len - 1] == '>') {
		inner++;
		inner_len -= 2;
	}
	if(inner_len <= 0 || parse_uri((char *)inner, inner_len, &puri) < 0) {
		LM_ERR("stored scontact failed to parse [%.*s]\n", src_len, src);
		return -1;
	}
	if(puri.host.len <= 0) {
		LM_ERR("stored scontact has no host [%.*s]\n", src_len, src);
		return -1;
	}

	/* FQDN host keep it as is */
	if(!tps_host_is_ip(&puri.host)) {
		LM_DBG("stored scontact host [%.*s] is a name, not refreshing\n",
				puri.host.len, puri.host.s);
		return 1;
	}

	/* host:port already match; node did not roam */
	if(tps_str_eq(&puri.host, host) && tps_str_eq(&puri.port, port)) {
		return 1;
	}

	if(buf_len < 2) {
		LM_ERR("buffer too small for <> framing\n");
		return -1;
	}
	buf[0] = '<';
	n = tps_swap_uri_hostport(
			inner, inner_len, &puri, host, port, buf + 1, buf_len - 2);
	if(n < 0) {
		LM_ERR("buffer too small rebuilding scontact\n");
		return -1;
	}
	buf[1 + n] = '>';
	out->s = buf;
	out->len = 1 + n + 1;
	return 0;
}

/**
 * Rebuild a stored RR-value by swapping the host:port of each entry that
 * carries ;sn=<sockname> for the host:port of the local socket resolved
 * from that sockname.
 *
 * A stored host that is a FQDN is skipped or just copied as is
 *
 * Returns 0 when the rebuilt value differs from the source.
 * Returns 1 when no entry needed rebuilding (rebuilt == source).
 * Returns -1 on buffer overflow / unrecoverable parse error.
 */
static int tps_rebuild_srr_from_sn(rr_t *head, char *buf, int buf_len, str *out)
{
	rr_t *cur;
	struct sip_uri puri;
	str sn_name = str_init("sn");
	str sn;
	socket_info_t *si;
	str *host;
	str *port;
	const char *entry_s;
	int entry_len;
	int n;
	int sep;
	int wrote = 0;
	int changed = 0;
	int entry_idx = 0;

	if(head == NULL || buf == NULL || out == NULL || buf_len <= 0) {
		return -1;
	}

	/* here check if rebuilt s_rr would equal stored one, byte-for-byte */
	for(cur = head; cur != NULL; cur = cur->next) {
		if(tps_rr_entry_needs_rebuild(cur)) {
			break;
		}
	}
	if(cur == NULL) {
		LM_DBG("s_rr refresh: no entry needs rebuild - skip\n");
		return 1;
	}

	/* here at least one entry will be rebuilt - assemble full output. */
	for(cur = head; cur != NULL; cur = cur->next) {
		sn.s = NULL;
		sn.len = 0;
		si = NULL;
		host = NULL;
		port = NULL;
		entry_s = cur->nameaddr.name.s;
		entry_len = cur->nameaddr.len;
		sep = (wrote > 0) ? 1 : 0;

		entry_idx++;

		/* Resolve ;sn= to a local socket. On any failure, fall through
		 * to the verbatim-copy path below. */
		if(parse_uri(cur->nameaddr.uri.s, cur->nameaddr.uri.len, &puri) == 0
				&& tps_host_is_ip(&puri.host)
				&& tps_get_param_value(&puri.params, &sn_name, &sn) == 0
				&& sn.len > 0 && (si = ksr_get_socket_by_name(&sn)) != NULL
				&& si_get_signaling_data(si, &host, &port) == 0 && host != NULL
				&& port != NULL && host->len > 0 && port->len > 0
				&& !(tps_str_eq(&puri.host, host)
						&& tps_str_eq(&puri.port, port))) {
			LM_DBG("rr entry #%d: sn=[%.*s] swap %.*s:%.*s -> %.*s:%.*s\n",
					entry_idx, sn.len, sn.s, puri.host.len, ZSW(puri.host.s),
					puri.port.len, ZSW(puri.port.s), host->len, host->s,
					port->len, port->s);
			if(sep) {
				if(wrote + 1 > buf_len)
					goto overflow;
				buf[wrote++] = ',';
			}
			n = tps_swap_uri_hostport(entry_s, entry_len, &puri, host, port,
					buf + wrote, buf_len - wrote);
			if(n < 0)
				goto overflow;
			wrote += n;
			changed = 1;
			continue;
		}

		/* Verbatim copy: no ;sn=, sockname unresolvable, or host:port
		 * already match the stored value. */
		LM_DBG("rr entry #%d: copy verbatim [%.*s]\n", entry_idx, entry_len,
				entry_s);
		if(wrote + sep + entry_len > buf_len)
			goto overflow;
		if(sep)
			buf[wrote++] = ',';
		memcpy(buf + wrote, entry_s, entry_len);
		wrote += entry_len;
	}

	if(!changed || wrote <= 0) {
		return 1;
	}

	/* `changed` is set only when at least one entry was rebuilt with a
	 * host:port that differs from the stored value. */
	out->s = buf;
	out->len = wrote;
	return 0;

overflow:
	LM_ERR("buffer too small rebuilding rr (buf_len=%d)\n", buf_len);
	return -1;
}

/**
 * Refresh stored {a,b}s_contact IP/port based on the stored ;sn= param
 */
void tps_refresh_scontacts_from_sn(
		tps_data_t *mtsd, tps_data_t *stsd, rr_t *srr)
{
	str sn1 = STR_NULL;
	str sn2 = STR_NULL;
	str *sn_for_a;
	int has2 = 0;
	str out;
	char *wbuf;
	int wlen;
	int rc;
	int has_bs;
	int has_as;

	if(mtsd == NULL || stsd == NULL) {
		return;
	}
	has_bs = (stsd->bs_contact.len > 0);
	has_as = (stsd->as_contact.len > 0);

	if(srr == NULL) {
		LM_DBG("no stored s_rr - skip scontact refresh\n");
		return;
	}
	if(!has_bs && !has_as) {
		LM_DBG("no stored scontacts - skip refresh\n");
		return;
	}

	/* First entry covers bs_contact; second (if any) covers as_contact.
	 * For single-RR we reuse sn1 for as_contact as well. */
	if(tps_rr_entry_sn(srr, &sn1) != 0) {
		LM_DBG("no ;sn= on first stored s_rr entry - skip refresh\n");
		return;
	}
	has2 = (tps_rr_entry_sn(srr->next, &sn2) == 0);

	if(has_bs) {
		tps_data_write_buf(mtsd, &wbuf, &wlen);
		rc = tps_rebuild_scontact_from_sn(
				&sn1, &stsd->bs_contact, wbuf, wlen, &out);
		if(rc == 0) {
			mtsd->bs_contact = out;
			mtsd->cp += out.len;
		}
	}
	if(has_as) {
		sn_for_a = has2 ? &sn2 : &sn1;
		tps_data_write_buf(mtsd, &wbuf, &wlen);
		rc = tps_rebuild_scontact_from_sn(
				sn_for_a, &stsd->as_contact, wbuf, wlen, &out);
		if(rc == 0) {
			mtsd->as_contact = out;
			mtsd->cp += out.len;
		}
	}
}

/**
 * Refresh the stored self-RR (s_rr) IP/port based on stored ;sn= param
 */
void tps_refresh_srr_from_sn(tps_data_t *mtsd, rr_t *srr)
{
	str out;
	char *wbuf;
	int wlen;
	int rc;

	if(srr == NULL) {
		LM_DBG("no stored s_rr - skip srr refresh\n");
		return;
	}

	tps_data_write_buf(mtsd, &wbuf, &wlen);
	rc = tps_rebuild_srr_from_sn(srr, wbuf, wlen, &out);
	if(rc == 0) {
		mtsd->s_rr = out;
		mtsd->cp += out.len;
	}
}
