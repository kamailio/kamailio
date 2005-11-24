#ifndef __REFERENCE_CNTR_H
#define __REFERENCE_CNTR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cds/sync.h>
	
typedef struct {
	int cntr;
	cds_mutex_t *mutex;
} reference_counter_data_t;
	
/* these functions can be called only by owner of at least one reference */
/* owner is somebody who:
 *    - created a referenced structure
 *    - added a reference
 *    - got a reference by an ovner
 */

void init_reference_counter(reference_counter_data_t *ref);
void add_reference(reference_counter_data_t *ref);
int get_reference_count(reference_counter_data_t *ref);

/* returns:
 * 0 if reference removed, but exist other references
 * 1 if removed last refernce and the element SHOULD be freed
 *
 * usage:
 * 
 * some_structure *ss;
 * ...
 * if (remove_reference(&ss->ref)) cds_free(&ss->ref);
 *  */
int remove_reference(reference_counter_data_t *ref);

#ifdef __cplusplus
}
#endif

#endif
