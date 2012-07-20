#include "../../sr_module.h"
#include "../../crc.h"

#include <ctype.h>

#include "sipcapture.h"
#include "hash_mode.h"

static int get_source(struct _sipcapture_object *sco, enum hash_source source,
                            str *source_string);
static int get_call_id (struct _sipcapture_object *sco, str *source_string);
static int get_from_user (struct _sipcapture_object *sco, str *source_string);
static int get_to_user (struct _sipcapture_object *sco, str *source_string);


static int first_token (str *source_string);


int hash_func (struct _sipcapture_object * sco,
                         enum hash_source source, int denominator) {
	int ret;
	unsigned int hash;
	str source_string;

	if(get_source (sco, source, &source_string) == -1) {
		return -1;
	}

	LM_DBG("source string: [%.*s]\n", source_string.len, source_string.s);
	crc32_uint(&source_string, &hash);

	ret = hash % denominator;
	return ret;
}

static int get_source (struct _sipcapture_object *sco, enum hash_source source,
                             str *source_string) {
	source_string->s = NULL;
	source_string->len = 0;

	switch (source) {
			case hs_call_id:
			return get_call_id (sco, source_string);
			case hs_from_user:
			return get_from_user(sco, source_string);
			case hs_to_user:
			return get_to_user(sco, source_string);
			default:
			LM_ERR("unknown hash source %i.\n",
			     (int) source);
			return -1;
	}
}

static int get_call_id (struct _sipcapture_object * sco, str *source_string) {

	if (!sco->callid.s || !sco->callid.len)
	{
		return -1;
	}
	source_string->s = sco->callid.s;
	source_string->len = sco->callid.len;
	first_token (source_string);
	return 0;
}

static int get_from_user (struct _sipcapture_object * sco, str *source_string) {

	if (!sco->from_user.s || !sco->from_user.len)
	{
		return -1;
	}
	source_string->s = sco->from_user.s;
	source_string->len = sco->from_user.len;
	return 0;
}


static int get_to_user (struct _sipcapture_object * sco, str *source_string) {

	if (!sco->to_user.s || !sco->to_user.len)
	{
		return -1;
	}
	source_string->s = sco->to_user.s;
	source_string->len = sco->to_user.len;
	return 0;
}


static int first_token (str *source_string) {
	size_t len;

	if (source_string->s == NULL || source_string->len == 0) {
		return 0;
	}

	while (source_string->len > 0 && isspace (*source_string->s)) {
		++source_string->s;
		--source_string->len;
	}
	for (len = 0; len < source_string->len; ++len) {
		if (isspace (source_string->s[len])) {
			source_string->len = len;
			break;
		}
	}
	return 0;
}


enum hash_source get_hash_source (const char *hash_source){

	if (strcasecmp ("call_id", hash_source) == 0){
		return hs_call_id;
	}
	else if (strcasecmp("from_user", hash_source) == 0)
	{
		return hs_from_user;
	}
	else if (strcasecmp("to_user", hash_source) == 0)
	{
		return hs_to_user;
	}
	else {
		return hs_error;
	}

}
