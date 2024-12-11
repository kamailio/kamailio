#ifndef _PRES_DFKS_H_
#define _PRES_DFKS_H_
#include "../../modules/sl/sl.h"

#include "../presence/bind_presence.h"
#include "../pua/pua_bind.h"
#include "../pua/pidf.h"

extern add_event_t pres_add_event;
extern sl_api_t slb;
extern pua_api_t pua;
extern struct tm_binds tmb;
extern presence_api_t pres;
extern pua_api_t pua;
extern libxml_api_t libxml_api;
extern str outbound_proxy;

#endif
