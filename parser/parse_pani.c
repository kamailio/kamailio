#include "../mem/mem.h"
#include "parse_pani.h"

int parse_pani_header(struct sip_msg * const msg) {
    pani_body_t* pani_body;
    struct hdr_field *hdr;
    char *p, *network_info = 0;
    int network_info_len = 0;
    int len;

    /* maybe the header is already parsed! */
    if (msg->pani && msg->pani->parsed)
	return 0;

    if (parse_headers(msg, HDR_EOH_F, 0) == -1 || !msg->pani) {
	return -1;
    }

    hdr = msg->pani;
    p = hdr->body.s;
    p = strchr(p, ';');
    if (p) {
	network_info = p + 1;
	network_info_len = hdr->body.len - (network_info - hdr->body.s) + 1/*nul*/;
    }

    len = sizeof(pani_body_t) + network_info_len;
    pani_body = (pani_body_t*) pkg_malloc(len);
    if (!pani_body) {
	LM_ERR("no more pkg mem\n");
	return -1;
    }
    memset(pani_body, 0, len);

    if (network_info_len > 0) {
	p = (char*) (pani_body + 1);
	memcpy(p, network_info, network_info_len);
	pani_body->access_info.s = p;
	pani_body->access_info.len = network_info_len;
    }

    if (strncmp(hdr->body.s, "IEEE-802.11a", 12) == 0) {
	pani_body->access_type = IEEE_80211a;
    } else if (strncmp(hdr->body.s, "IEEE-802.11a", 12) == 0) {
	pani_body->access_type = IEEE_80211b;
    } else if (strncmp(hdr->body.s, "3GPP-GERAN", 10) == 0) {
	pani_body->access_type = _3GPP_GERAN;
    } else if (strncmp(hdr->body.s, "3GPP-UTRAN-FDD", 14) == 0) {
	pani_body->access_type = _3GPP_UTRANFDD;
    } else if (strncmp(hdr->body.s, "3GPP-UTRAN-TDD", 14) == 0) {
	pani_body->access_type = _3GPP_EUTRANTDD;
    } else if (strncmp(hdr->body.s, "3GPP-E-UTRAN-FDD", 16) == 0) {
	pani_body->access_type = _3GPP_EUTRANFDD;
    } else if (strncmp(hdr->body.s, "3GPP-E-UTRAN-TDD", 16) == 0) {
	pani_body->access_type = _3GPP_UTRANTDD;
    } else if (strncmp(hdr->body.s, "3GPP-CDMA2000", 13) == 0) {
	pani_body->access_type = _3GPP_CDMA_2000;
    } else {
	LM_ERR("Unknown access type [%.*s]\n", hdr->body.len, hdr->body.s);
	return -1;
    }
    hdr->parsed = (void*) pani_body;

    return 0;
}

int free_pani_body(struct pani_body *body) {
    if (body != NULL) {
	pkg_free(body);
    }
    return 0;
}