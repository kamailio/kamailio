#include "../parser/sdp/sdp.h"
#include "../parser/parse_uri.c"
#include "../parser/parse_hname2.h"
#include "../parser/contact/parse_contact.h"
#include "../parser/parse_from.h"
#include "../parser/parse_to.h"
#include "../parser/parse_refer_to.h"
#include "../parser/parse_ppi_pai.h"
#include "../parser/parse_privacy.h"
#include "../parser/parse_diversion.h"
#include "../parser/parse_identityinfo.h"
#include "../parser/parse_disposition.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    ksr_hname_init_index();
    return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    sip_msg_t orig_inv = { };
    orig_inv.buf = (char*)data;
    orig_inv.len = size;

    if (parse_msg(orig_inv.buf, orig_inv.len, &orig_inv) < 0) {
        goto cleanup;
    }

    parse_headers(&orig_inv, HDR_EOH_F, 0);

    parse_sdp(&orig_inv);

    parse_from_header(&orig_inv);

    parse_from_uri(&orig_inv);

    parse_to_header(&orig_inv);

    parse_to_uri(&orig_inv);

    parse_contact_header(&orig_inv);

    parse_refer_to_header(&orig_inv);

    parse_pai_header(&orig_inv);

    parse_diversion_header(&orig_inv);

    parse_privacy(&orig_inv);

    parse_content_disposition(&orig_inv);

    parse_identityinfo_header(&orig_inv);

    str uri;
    get_src_uri(&orig_inv, 0, &uri);

    str ssock;
    get_src_address_socket(&orig_inv, &ssock);

cleanup:
    free_sip_msg(&orig_inv);

    return 0;
}
