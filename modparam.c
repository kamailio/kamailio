#include "modparam.h"
#include "dprint.h"
#include <string.h>


int set_mod_param(char* _mod, char* _name, modparam_t _type, void* _val)
{
	void* ptr;
	
	if (!_mod) {
		LOG(L_ERR, "set_mod_param(): Invalid _mod parameter value\n");
		return -1;
	}

	if (!_name) {
		LOG(L_ERR, "set_mod_param(): Invalid _name parameter value\n");
		return -2;
	}

	ptr = find_param_export(_mod, _name, _type);
	if (!ptr) {
		LOG(L_ERR, "set_mod_param(): Parameter not found\n");
		return -3;
	}

	switch(_type) {
	case STR_PARAM:
		*((char**)ptr) = strdup((char*)_val);
		break;

	case INT_PARAM:
		*((int*)ptr) = (int)_val;
		break;
	}

	return 0;
}
