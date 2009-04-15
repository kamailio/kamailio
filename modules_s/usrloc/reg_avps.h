#ifndef __REG_AVPS_H
#define __REG_AVPS_H

#define DEFAULT_REG_AVPFLAG_NAME	"regavps"

#include "../../usr_avp.h"
#include "../../str.h"
#include "../../lib/srdb2/db_con.h"
#include "../usrloc/udomain.h"

int save_reg_avps(struct ucontact* c);
int dup_reg_avps(struct ucontact *dst, struct ucontact *src);
int delete_reg_avps(struct ucontact* c);

int set_reg_avpflag_name(char *name);

int read_reg_avps(struct sip_msg *m, char*, char*);
int read_reg_avps_fixup(void** param, int param_no);


/* module parameters (column and table names and DB control) */
/* set serialized_reg_avp_column to non-NULL & non-empty value to
 * allow AVPs to be serialized into given column in location table 
 * (better will be extra numeric switch, but ...) */
extern char *avp_column;

/* returns 1 if reg AVPs are used, 0 if not */
int use_reg_avps();

#endif
