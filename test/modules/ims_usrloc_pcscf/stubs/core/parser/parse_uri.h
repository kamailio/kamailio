/* Test-only stand-in for src/core/parser/parse_uri.h.
 *
 * The real header pulls in msg_parser.h and the rest of the SIP message
 * parser, which a standalone unit test binary cannot link against. Only
 * the fields impu_match.c actually reads (user/host) are provided here;
 * test_parse_uri_stub.c supplies the matching parse_uri() implementation.
 * Reached only via the staged copy of impu_match.c (see stubs/modules/),
 * whose relative "../../core/parser/parse_uri.h" include resolves here.
 */
#ifndef PCSCF_STUB_PARSE_URI_H
#define PCSCF_STUB_PARSE_URI_H

struct sip_uri
{
	int type;
	str user;
	str host;
};

int parse_uri(char *buf, int len, struct sip_uri *uri);

#endif /* PCSCF_STUB_PARSE_URI_H */
