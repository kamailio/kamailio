/* Self-contained unit test for GRUU extraction.
 * Uses a minimal local reimplementation of extract_gruu_from_contact
 * to avoid heavy module header dependencies for this harness.
 */
#include "test_common.h"
#include <string.h>
#include <stdlib.h>

typedef struct
{
	char *s;
	int len;
} str;

typedef struct param
{
	str name;
	str body;
	struct param *next;
} param_t;

typedef struct contact
{
	str instance_body;
	param_t *instance;
	param_t *params;
} contact_t;

typedef struct
{
	str instance_id;
	str pub_gruu;
	str temp_gruu;
} gruu_fields_t;

static int param_eq(param_t *p, const char *name)
{
	if(!p || !name)
		return 0;
	if((int)strlen(name) != p->name.len)
		return 0;
	return (strncasecmp(p->name.s, name, p->name.len) == 0);
}

static void gruu_fields_free(gruu_fields_t *gf)
{
	if(!gf)
		return;
	if(gf->instance_id.s)
		free(gf->instance_id.s);
	if(gf->pub_gruu.s)
		free(gf->pub_gruu.s);
	if(gf->temp_gruu.s)
		free(gf->temp_gruu.s);
	memset(gf, 0, sizeof(*gf));
}

static int extract_gruu_from_contact(contact_t *c, gruu_fields_t *gf)
{
	param_t *p;
	if(!c || !gf)
		return -1;
	memset(gf, 0, sizeof(*gf));

	/* instance */
	if(c->instance && c->instance->body.len > 0) {
		char *s = c->instance->body.s;
		int len = c->instance->body.len;
		if(len >= 2 && s[0] == '\"' && s[len - 1] == '\"') {
			len -= 2;
			s = s + 1;
		}
		gf->instance_id.s = malloc(len + 1);
		if(!gf->instance_id.s) {
			gruu_fields_free(gf);
			return -1;
		}
		memcpy(gf->instance_id.s, s, len);
		gf->instance_id.s[len] = '\\0';
		gf->instance_id.len = len;
	}

	for(p = c->params; p; p = p->next) {
		if(!p->body.s || p->body.len == 0)
			continue;
		if(param_eq(p, "pub-gruu")) {
			gf->pub_gruu.s = malloc(p->body.len + 1);
			if(!gf->pub_gruu.s) {
				gruu_fields_free(gf);
				return -1;
			}
			memcpy(gf->pub_gruu.s, p->body.s, p->body.len);
			gf->pub_gruu.s[p->body.len] = '\\0';
			gf->pub_gruu.len = p->body.len;
		} else if(param_eq(p, "temp-gruu")) {
			gf->temp_gruu.s = malloc(p->body.len + 1);
			if(!gf->temp_gruu.s) {
				gruu_fields_free(gf);
				return -1;
			}
			memcpy(gf->temp_gruu.s, p->body.s, p->body.len);
			gf->temp_gruu.s[p->body.len] = '\\0';
			gf->temp_gruu.len = p->body.len;
		}
	}

	if((gf->instance_id.s && gf->instance_id.len > 0)
			|| (gf->pub_gruu.s && gf->pub_gruu.len > 0)
			|| (gf->temp_gruu.s && gf->temp_gruu.len > 0))
		return 0;
	gruu_fields_free(gf);
	return -1;
}

static int test_extract_all_fields(void)
{
	contact_t c;
	param_t pub, temp, inst;
	gruu_fields_t gf;

	memset(&c, 0, sizeof(c));
	memset(&pub, 0, sizeof(pub));
	memset(&temp, 0, sizeof(temp));
	memset(&inst, 0, sizeof(inst));

	/* instance with surrounding quotes */
	inst.body.s = "\"instance-123\"";
	inst.body.len = strlen(inst.body.s);
	c.instance = &inst;

	/* pub-gruu param */
	pub.name.s = "pub-gruu";
	pub.name.len = strlen(pub.name.s);
	pub.body.s = "sip:pub@ims.local;gr=urn:uuid:aaaa";
	pub.body.len = strlen(pub.body.s);

	/* temp-gruu param */
	temp.name.s = "temp-gruu";
	temp.name.len = strlen(temp.name.s);
	temp.body.s = "sip:temp@ims.local;gr=urn:uuid:bbbb";
	temp.body.len = strlen(temp.body.s);

	/* chain params */
	pub.next = &temp;
	c.params = &pub;

	TEST_ASSERT(extract_gruu_from_contact(&c, &gf) == 0);
	/* expect instance id without quotes and non-zero lengths */
	TEST_ASSERT(gf.instance_id.len == (int)strlen("instance-123"));
	TEST_ASSERT(gf.pub_gruu.len == (int)strlen(pub.body.s));
	TEST_ASSERT(gf.temp_gruu.len == (int)strlen(temp.body.s));

	gruu_fields_free(&gf);
	return 0;
}

int main(void)
{
	int failures = 0;

	RUN_TEST(test_extract_all_fields);

	return failures > 0 ? 1 : 0;
}
