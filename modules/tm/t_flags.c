/*
 * $Id$
 */


#include "t_funcs.h"

int t_setflag( unsigned int flag ) {
	T->flags |= 1 << flag;
	return 1;
}

int t_resetflag( unsigned int flag ) {
	T->flags &= ~ flag;
	return 1;
}

int t_isflagset( unsigned int flag ) {
	return T->flags & (1<<flag) ? 1 : -1;
}
