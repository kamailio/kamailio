#include "../../core/dprint.h"

#include "ims_usrloc_pcscf_mod.h"
#include "usrloc.h"

extern int db_mode;
extern int db_cleanup_temp_gruu_history(void);

void ul_timer_cleanup_temp_gruu_history(void)
{
	if(db_mode == NO_DB)
		return;

	if(db_cleanup_temp_gruu_history() < 0) {
		LM_WARN("temp GRUU history cleanup failed\n");
	}
}
