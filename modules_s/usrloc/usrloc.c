/* $Id$
 *
 * User location support
 *
 */


#include "usrloc.h"
#include "../../dprint.h"
#include "cache.h"
#include "utils.h"

#define HDR_FROM 10
#define HDR_CONTACT 11
#define HDR_EXPIRES 12


static int processContact_impl (struct sip_msg*, char*, char*);
static int rewriteFromSQL_impl (struct sip_msg*, char*, char*);

void destroy(void);
char* get_to(struct sip_msg* _msg);
int rw(struct sip_msg* _msg, const char* _val);

static struct module_exports print_exports= {	"usrloc", 
						(char*[]) {
							"processContact",
							"rewriteFromSQL"
						},
						(cmd_function[]) {
							processContact_impl, 
							rewriteFromSQL_impl
						},
						(int[]){1, 1},
						(fixup_function[]){0, 0},
						2,
						0,
						destroy,
						0 /* oncancel function */
};


static cache_t* c;


struct module_exports* mod_register()
{
	printf( "Registering user location module\n");
	if (db_init("sql://localhost/ser") == FALSE) {
		LOG(L_ERR, "mod_register(): Unable to initialize database connection\n");
	}

 	c = create_cache(512);
	if (c == NULL) {
		LOG(L_ERR, "mod_register(): Unable to create cache\n");
	}

	return &print_exports;
}


void destroy(void)
{
	free_cache(c);
	db_close();
}




static int processContact_impl(struct sip_msg* _msg, char* _str1, char* _str2)
{
	location_t* loc;
#ifdef PARANOID
	if (!_msg) return -1;
	if (!_str1) return -1;
#endif
	loc = msg2loc(_msg);

	if (!loc) {
		LOG(L_ERR, "processContact(): Unable to convert SIP message to location_t\n");
		return -1;
	}

	if (cache_put(c, loc) == FALSE) {
		LOG(L_ERR, "processContact(): Unable to put location into cache\n");
		free_location(loc);
		return -1;
	}

	return 1;
}



static int rewriteFromSQL_impl(struct sip_msg* _msg, char* _str1, char* _str2)
{
	location_t* loc;
	char* to;
	c_elem_t* el;
#ifdef PARANOID
	if (!_msg) return -1;
	if (!_str1) return -1;
#endif
	
	if (parse_headers(_msg, HDR_TO) == -1) {
		LOG(L_ERR, "rewriteFromSQL(): Error while parsing headers\n");
		return -1;
	} else {
		if (!_msg->to) {
			LOG(L_ERR, "rewriteFromSQL(): Unable to find To header field\n");
			return -1;
		}

		to = parse_to(_msg->to->name.s);

		if (to == NULL) {
			LOG(L_ERR, "rewriteFromSQL(): Unable to get URI\n");
			return -1;
		}
		
		el = cache_get(c, to);
		
		if (el) {
			if (rw(_msg, el->loc->contacts->c.s) == FALSE) {
				LOG(L_ERR, "rewriteFromSQL(): Unable to rewrite message\n");
			}
			cache_release_elem(el);
			return 1;
		} else {
			return 0;
		}
	}
}



int rw(struct sip_msg* _msg, const char* _val)
{
#ifdef PARANOID
	if (!_msg) return FALSE;
	if (!_val) return FALSE;
#endif

	


	return TRUE;
}
