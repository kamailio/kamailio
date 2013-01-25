#ifndef PXML_API_H
#define PXML_API_H
#include "../../str.h"

typedef int (*pres_check_basic_t)(struct sip_msg*, str presentity_uri, str status);
typedef int (*pres_check_activities_t)(struct sip_msg*, str presentity_uri, str activity);

typedef struct presence_xml_binds {
	pres_check_basic_t pres_check_basic;
	pres_check_activities_t pres_check_activities;
} presence_xml_api_t;

typedef int (*bind_presence_xml_f)(presence_xml_api_t*);

int bind_presence_xml(struct presence_xml_binds*);

inline static int presence_xml_load_api(presence_xml_api_t *pxb)
{
	bind_presence_xml_f bind_presence_xml_exports;
	if (!(bind_presence_xml_exports = (bind_presence_xml_f)find_export("bind_presence_xml", 1, 0)))
	{
		LM_ERR("Failed to import bind_presence_xml\n");
		return -1;
	}
	return bind_presence_xml_exports(pxb);
}

#endif /*PXML_API_H*/
