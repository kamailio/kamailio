#include "db_con.h"
#include "defs.h"
#include "../../mem.h"
#include "../../dprint.h"


int use_table(db_con_t* _h, const char* _t)
{
	char* ptr;
#ifdef PARANOID
	if (!_h) return FALSE;
	if (!_t) return FALSE;
#endif
	ptr = (char*)pkg_malloc(strlen(_t) + 1);
	if (!ptr) {
		log(L_ERR, "use_table(): No memory left\n");
		return FALSE;
	}

	if (_h->table) pkg_free(_h->table);
	_h->table = ptr;
	return TRUE;
}


