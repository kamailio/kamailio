#include "../parser/parse_uri.c"

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    struct sip_uri uri;
    parse_uri(data, size, &uri);
    return 0;
}
