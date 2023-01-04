
#include "../config.h"
#include "../parser/parse_uri.c"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    struct sip_uri uri;
    if(size >= BUF_SIZE) {
        /* test with larger message than core accepts, but not indefinitely large */
        return 0;
    }
    parse_uri(data, size, &uri);
    return 0;
}
