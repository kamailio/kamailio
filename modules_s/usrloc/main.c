/* 
 * $Id$ 
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "location.h"
#include "contact.h"
//#include "dbcon_mysql.h"
#include "cache.h"
#include "timer_new.h"
#include "timer_dirty.h"

#define TRUE 1
#define FALSE 0


int rewriteFromSQL(cache_t* _c, const char* _URI, char* _dest)
{
	c_elem_t* ptr;
#ifdef PARANOID
	if (!_c) return FALSE;
	if (!_URI) return FALSE;
	if (!_dest) return FALSE;
#endif
	ptr = cache_get(_c, _URI);
	if (!ptr) {
		return FALSE;
	} else {
		strcpy(_dest, ptr->loc->contacts->c.s);
		cache_release_elem(ptr);
		return TRUE;
	}
}


int processContact(cache_t* _c, const char* _to, const char* _contact, float q)
{
        int expires_def = 3600;
	location_t* loc;
#ifdef PARANOID
	if (!_c) return FALSE;
	if (!_to) return FALSE;
	if (!_contact) return FALSE;
#endif

	loc = create_location(_to, FALSE, expires_def);
	if (!loc) return FALSE;

	add_contact(loc, _contact, expires_def, q, TRUE, FALSE);

	cache_put(_c, loc);
	
	return TRUE;
}


/*
int main(int argc, char* argv[])
{
	int i;
	cache_t* c;
	location_t* loc, *loc2;
	c_elem_t* ptr;
	char cmd[1024];
	char to[256], contact[256];
	float q;

	db_init("sql://localhost/ser");
	init_timer_new("location");
	init_timer_dirty("location");

	c = create_cache(512);
	//	processContact(c, "J.Janak@sh.cvut.cz", "janakj@devitka.sh.cvut.cz");
	//	processContact(c, "J.Janak@sh.cvut.cz", "janakj2@devitka.sh.cvut.cz");

      
	for(i = 0; i < 200000; i++) {
		     //query_location("location", "J.Janak@sh.cvut.cz", &loc);
		rewriteFromSQL(c, "J.Janak@sh.cvut.cz", contact);
	}
       

	while(fgets(cmd, 256, stdin) != NULL) {
		if (cmd[0] == 'r') {
			sscanf(cmd, "g %s\n", to);
			rewriteFromSQL(c, to, contact);
			printf("rewriteFromSQL=%s\n", contact);
		}
		if (cmd[0] == 'p') {
			sscanf(cmd, "p %s %s %f\n", to, contact, &q);
			processContact(c, to, contact, q);
			printf("processContact done\n");
		}

		if (cmd[0] == 'd') {
			print_cache(c);
		}

		if (cmd[0] == '1') {
			timer_new(c);
			printf("timer_new() done\n");
		}

		if (cmd[0] == '2') {
			timer_dirty(c);
			printf("timer_dirty() done \n");
		}
		
		printf("cmd>");
		fflush(stdout);
	}

	free_cache(c);


	close_timer_new();
	close_timer_dirty();
	db_close();

	return EXIT_SUCCESS;
}
*/
