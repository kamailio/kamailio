#include <stdint.h>
#include <string.h>

#include "../config.h"

#ifndef HAVE_GETHOSTBYNAME2
#define HAVE_GETHOSTBYNAME2 1
#endif

#include "../resolve.h"
#include "../ip_addr.h"
#include "../mem/mem.h"

/* These are not static in resolve.c, but no header declares them, so they are
 * declared here rather than by touching a core header for a test target. */
struct srv_rdata *dns_srv_parser(unsigned char *msg, unsigned char *end,
		unsigned char *eor, unsigned char *rdata);
struct naptr_rdata *dns_naptr_parser(unsigned char *msg, unsigned char *end,
		unsigned char *eor, unsigned char *rdata);
struct cname_rdata *dns_cname_parser(
		unsigned char *msg, unsigned char *end, unsigned char *rdata);
struct a_rdata *dns_a_parser(unsigned char *rdata, unsigned char *eor);
struct aaaa_rdata *dns_aaaa_parser(unsigned char *rdata, unsigned char *eor);
struct ptr_rdata *dns_ptr_parser(
		unsigned char *msg, unsigned char *end, unsigned char *rdata);
unsigned char *dns_skipname(unsigned char *p, unsigned char *end);

#define kDnsRdataOffset 12
#define kMinInputLength (kDnsRdataOffset + 2)
#define kMaxInputLength 65535

static struct naptr_rdata *parse_one_record(unsigned char *msg,
		unsigned char *end, unsigned char *eor, unsigned char *rdata,
		char *naptr_proto)
{
	struct srv_rdata *srv;
	struct naptr_rdata *naptr;
	struct cname_rdata *cname;
	struct a_rdata *a;
	struct aaaa_rdata *aaaa;
	struct ptr_rdata *ptr;

	srv = dns_srv_parser(msg, end, eor, rdata);
	if(srv != NULL) {
		pkg_free(srv);
	}

	cname = dns_cname_parser(msg, end, rdata);
	if(cname != NULL) {
		pkg_free(cname);
	}

	a = dns_a_parser(rdata, eor);
	if(a != NULL) {
		pkg_free(a);
	}

	aaaa = dns_aaaa_parser(rdata, eor);
	if(aaaa != NULL) {
		pkg_free(aaaa);
	}

	ptr = dns_ptr_parser(msg, end, rdata);
	if(ptr != NULL) {
		pkg_free(ptr);
	}

	dns_skipname(rdata, end);

	*naptr_proto = 0;
	naptr = dns_naptr_parser(msg, end, eor, rdata);
	if(naptr != NULL) {
		*naptr_proto = naptr_get_sip_proto(naptr);
		naptr_proto_supported(*naptr_proto);
		naptr_proto_preferred(*naptr_proto, PROTO_UDP);
	}

	return naptr;
}

static void run_host_helpers(char *s, int len)
{
	str host;
	ip_addr_t ipb;
	char srv_name[MAX_DNS_NAME];

	if(len <= 0) {
		return;
	}

	host.s = s;
	host.len = len;

	str2ipbuf(&host, &ipb);
	str2ip6buf(&host, &ipb);
	str2ipxbuf(&host, &ipb);

	str2ip(&host);
	str2ip6(&host);
	str2ipx(&host);

	if((host.len + SRV_MAX_PREFIX_LEN + 1) <= MAX_DNS_NAME) {
		create_srv_name(PROTO_UDP, &host, srv_name);
		create_srv_name(PROTO_TCP, &host, srv_name);
		create_srv_name(PROTO_TLS, &host, srv_name);
		create_srv_name(PROTO_SCTP, &host, srv_name);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	unsigned char *buf;
	unsigned char *msg;
	unsigned char *end;
	unsigned char *rdata;
	unsigned char *eor;
	unsigned int rdlength;
	struct naptr_rdata *strict_naptr;
	struct naptr_rdata *loose_naptr;
	char strict_proto;
	char loose_proto;

	if(size < kMinInputLength || size > kMaxInputLength) {
		return 0;
	}

	buf = (unsigned char *)pkg_malloc(size);
	if(buf == NULL) {
		return 0;
	}
	memcpy(buf, data, size);

	msg = buf;
	end = buf + size;
	rdata = buf + kDnsRdataOffset;

	rdlength = ((unsigned int)buf[kDnsRdataOffset - 2] << 8)
			   | (unsigned int)buf[kDnsRdataOffset - 1];
	eor = rdata + rdlength;
	if(eor > end) {
		eor = end;
	}
	strict_naptr = parse_one_record(msg, end, eor, rdata, &strict_proto);

	loose_naptr = parse_one_record(msg, end, end, rdata, &loose_proto);

	if(strict_naptr != NULL && loose_naptr != NULL) {
		struct naptr_rdata *chosen = strict_naptr;
		char chosen_proto = strict_proto;

		naptr_choose(&chosen, &chosen_proto, loose_naptr, loose_proto);
	}

	if(strict_naptr != NULL) {
		pkg_free(strict_naptr);
	}
	if(loose_naptr != NULL) {
		pkg_free(loose_naptr);
	}

	run_host_helpers((char *)buf, (int)size);
	run_host_helpers((char *)rdata, (int)(size - kDnsRdataOffset));

	pkg_free(buf);

	return 0;
}
