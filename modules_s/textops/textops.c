/*$Id$
 *
 * Example ser module, it implements the following commands:
 * search_append("key", "txt") - insert a "txt" after "key"
 * replace("txt1", "txt2") - replaces txt1 with txt2 (txt1 can be a re)
 * search("txt") - searches for txt (txt can be a regular expression)
 * append_to_reply("txt") - appends txt to the reply?
 * append_hf("P-foo: bar\r\n");
 *
 * 
 */



#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include "../../mem/mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

static int search_f(struct sip_msg*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int search_append_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);
static int append_hf(struct sip_msg* msg, char* str1, char* str2);

static int fixup_regex(void**, int);
static int str_fixup(void** param, int param_no);

static int mod_init(void);


struct module_exports exports= {
	"textops",
	(char*[])	{
			"search",
			"search_append",
			"replace",
			"append_to_reply",
			"append_hf"
	},
	(cmd_function[]) {
			search_f,
			search_append_f,
			replace_f,
			append_to_reply_f,
			append_hf
	},
	(int[]) {
			1,
			2,
			2,
			1,
			1
	},
	(fixup_function[]){
			fixup_regex,
			fixup_regex,
			fixup_regex,
			0,
			str_fixup
	},
	5,

	0,      /* Module parameter names */
	0,      /* Module parameter types */
	0,      /* Module parameter variable pointers */
	0,      /* Number of module paramers */

	mod_init, /* module initialization function */
	0, /* response function */
	0,  /* destroy function */
	0, /* on_cancel function */
	0, /* per-child init function */
};


static int mod_init(void)
{
	fprintf(stderr, "%s - initializing\n", exports.name);
	return 0;
}


static int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	return 1;
}



static int search_append_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump* l;
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=anchor_lump(&msg->add_rm, pmatch.rm_eo, 0, 0))==0)
			return -1;
		return insert_new_lump_after(l, str, strlen(str), 0)?1:-1;
	}
	return -1;
}



static int replace_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump* l;
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=del_lump(&msg->add_rm, pmatch.rm_so,
						pmatch.rm_eo-pmatch.rm_so, 0))==0)
			return -1;
		return insert_new_lump_after(l, str, strlen(str), 0)?1:-1;
	}
	return -1;
}



static int fixup_regex(void** param, int param_no)
{
	regex_t* re;

	DBG("module - fixing %s\n", (char*)(*param));
	if (param_no!=1) return 0;
	if ((re=malloc(sizeof(regex_t)))==0) return E_OUT_OF_MEM;
	if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ){
		free(re);
		LOG(L_ERR, "ERROR: %s : bad re %s\n", exports.name, (char*)*param);
		return E_BAD_RE;
	}
	/* free string */
	free(*param);
	/* replace it with the compiled re */
	*param=re;
	return 0;
}



static int append_to_reply_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump_rpl *lump;

	lump = build_lump_rpl( key, strlen(key) );
	if (!lump)
	{
		LOG(L_ERR,"ERROR:append_to_reply : unable to create lump_rl\n");
		return -1;
	}
	add_lump_rpl( msg , lump );

	return 1;
}



static int append_hf(struct sip_msg* msg, char* str1, char* str2)
{
	struct lump* anchor;
	char *s;

	if (parse_headers(msg, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "append_hf(): Error while parsing message\n");
		return -1;
	}

	anchor = anchor_lump(&msg->add_rm, msg->unparsed - msg->buf, 0, 0);
	if (anchor == 0) {
		LOG(L_ERR, "append_hf(): Can't get anchor\n");
		return -1;
	}

	s = (char*)pkg_malloc(((str*)str1)->len);
	if (!s) {
		LOG(L_ERR, "append_hf(): No memory left\n");
	}

	memcpy(s, ((str*)str1)->s, ((str*)str1)->len);

	if (insert_new_lump_before(anchor, s, ((str*)str1)->len, 0) == 0) {
		LOG(L_ERR, "append_hf(): Can't insert lump\n");
		return -1;
	}

	return 1;
}


/*
 * Convert char* parameter to str* parameter
 */
static int str_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "str_fixup(): No memory left\n");
			return E_UNSPEC;
		}

		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}

	return 0;
}
