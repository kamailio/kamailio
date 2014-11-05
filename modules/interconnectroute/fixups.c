#include "fixups.h"


int uri_fixup(void ** param) {

    return 0;
}
int ix_trunk_query_fixup(void ** param, int param_no) {
	if (param_no == 1) {
	    return fixup_var_str_12(param, param_no);
	}

	return 0;
}