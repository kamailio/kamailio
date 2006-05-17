#include "uri_ops.h"
#include "../../id.h"
#include "../../parser/parse_from.h"
#include <cds/sstr.h>
#include <stdio.h>

int is_simple_rls_target(struct sip_msg *m, char *_template, char *unused)
{
	str from_uid;
	struct sip_uri furi, turi;
	str from_uri, to_uri;
	str tmp;
	static str sample = STR_STATIC_INIT("$uid");
	static str templ;
	int res = 1;

	PROF_START(rls_is_simple_rls_target)
	if (get_from_uid(&from_uid, m) < 0) {
		ERR("can't get From UID\n");
		PROF_STOP(rls_is_simple_rls_target)
		return -1;
	}
	
	if (_template) {
		templ.s = _template;
		templ.len = strlen(_template);
	}
	else {
		templ.s = NULL;
		templ.len = 0;
	}
	
	from_uri = get_from(m)->uri;
	to_uri = get_to(m)->uri;

	if (parse_uri(from_uri.s, from_uri.len, &furi) < 0) {
		LOG(L_ERR, "Error while parsing From URI\n");
		PROF_STOP(rls_is_simple_rls_target)
		return -1;
	}
	if (parse_uri(to_uri.s, to_uri.len, &turi) < 0) {
		LOG(L_ERR, "Error while parsing To URI\n");
		PROF_STOP(rls_is_simple_rls_target)
		return -1;
	}
	
	/* compare domains */
	if (str_nocase_equals(&turi.host, &furi.host) != 0) {
		/* not equal */
		DBG("different domains\n");
		PROF_STOP(rls_is_simple_rls_target)
		return -1;
	}

	/* compare usernames */
	if (replace_str(&templ, &tmp, &sample, &from_uid) < 0) {
		ERR("can't allocate memory\n");
		PROF_STOP(rls_is_simple_rls_target)
		return -1;
	}
	
	if (str_nocase_equals(&turi.user, &tmp) != 0) {
		/* not equal */
		DBG("template doesn't match\n");
		res = -1;
	}
	
	str_free_content(&tmp);
	
	PROF_STOP(rls_is_simple_rls_target)
	return res;
}

