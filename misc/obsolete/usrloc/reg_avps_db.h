#ifndef _REG_AVPS_DB_H
#define _REG_AVPS_DB_H

#include "../../str.h"
#include "../../usr_avp.h"

avp_t *deserialize_avps(str *serialized_avps);
int serialize_avps(avp_t *first, str *dst);

#endif /* _REG_AVPS_DB_H */
