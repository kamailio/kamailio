#ifndef _MODULES_PA_LOCATION_H_
#define _MODULES_PA_LOCATION_H_

int pa_location_init(void);

/*
 * lookup placeid corresponding to room name
 */
int location_lookup_placeid(str *room_name, int *placeid);

/*
 * Add a user information
 */
int location_doc_add_user(str* _b, int _l, str *user_uri);

/*
 * Create start of location document
 */
int location_doc_start(str* _b, int _l);

/*
 * Start a resource in a location document
 */
int location_doc_start_userlist(str* _b, int _l, str* _uri);

/*
 * End a userlist in a location document
 */
int location_doc_end_resource(str *_b, int _l);

/*
 * End a location document
 */
int location_doc_end(str* _b, int _l);


#endif /* _MODULES_PA_LOCATION_H_ */
