#ifdef SER

#include <cds/sstr.h>
#include "parser/msg_parser.h"
#include <cds/memory.h>

/* if set uri_only it extracts only uri, not full Contact header */
int extract_server_contact(struct sip_msg *m, str *dst, int uri_only);

#endif
