#ifndef __RLS_URI_OPS_H

#include "rls_mod.h"

/* URI functions for usage from CFG script */

/* tries to look on To URI/AOR according to given*/
int is_simple_rls_target(struct sip_msg *m, char *what, char *_template);

#endif
