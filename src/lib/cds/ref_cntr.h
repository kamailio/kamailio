#ifndef __REFERENCE_CNTR_H
#define __REFERENCE_CNTR_H

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup cds
 * @{ 
 *
 * \defgroup cds_ref_cnt Reference counting
 *
 * Experimental functions for reference counting (to simplify
 * this code elsewhere).
 * 
 * Reference counter (\ref reference_counter_data_t) holds integer number which
 * should be changed using functions \ref add_reference and \ref
 * remove_reference. Using these functions is the number read/changed from
 * locked section guarded by one mutex from a set of 'group mutexes'.
 *
 * Often used scenario:
 * - list of structures, change in the list (adding, removing) is guarded by
 *   one mutex, 
 * - each structure has reference counter, when the count is zero, the
 *   structure can be removed from the list and freed
 *
 * Note that mutex for adding references is needed because references are not
 * added from critical section guarded by main list mutex but can be added whenever.
 *
 * Typical usage:
 * \code
 * struct some_structure {
 *     ...
 *     reference_counter_data_t ref;
 *     ...
 * };
 *
 * reference_counter_group_t *some_grp;
 * 
 * void init()
 * {
 *     some_grp = init_reference_counter_group(16);
 * }
 *
 * void destroy()
 * {
 *     free_reference_counter_group(some_grp);
 * }
 * 
 * ...
 *
 * // adding a member:
 * struct some_struct *ss = malloc(...);
 * init_reference_counter(some_grp, &ss->ref);
 * lock(list);
 * add_to_list(ss);
 * unlock(list);
 *
 * ...
 * // adding a reference doesn't need to lock list
 * // can be done only by a reference owner
 * add_reference(&ss->ref);
 *
 * // releasing a member when not needed for caller and there is
 * // no way how to obtain reference for released member
 * // (no way how to find a member in list)
 * if (remove_reference(&ss->ref)) {
 *     // last reference removed
 *     lock(list);
 *     remove_from_list(ss);
 *     free(ss);
 *     unlock(list);
 * }
 *  
 * // or 
 * // releasing a member when not needed for caller and it is possible 
 * // to 'search' in the list (reference to released member can be somehow 
 * // obtained by inspecting the list):
 * lock(list);
 * if (remove_reference(&ss->ref)) {
 *     // last reference removed
 *     remove_from_list(ss);
 *     free(ss);
 * } 
 * unlock(list);
 * \endcode
 *
 * \todo use atomic operations instead of locking
 * @{ */

#include <cds/sync.h>

/** Structure holding reference counter value. */
typedef struct {
	int cntr; /**< counter value */
	cds_mutex_t *mutex; /**< mutex asigned to this reference counter */
} reference_counter_data_t;

/** Structure holding information about group of reference counters.  
 * It holds array of mutexes which are assigned to single reference 
 * counters in this group. The assignment is done using an operation 
 * on pointer to reference_counter_data_t. */
typedef struct {
	int mutex_cnt; /**< number of mutexes for this group */

	/** number of next mutex to be assigned - this member is NOT 
	 * read/changed from critical section but it doesn't matter
	 * if it will be rewritten between processes because it is used
	 * for load distributing only (the worst thing that can happen
	 * is that the same mutex is assigned twice) */
	int mutex_to_assign; 
	cds_mutex_t mutexes[1]; /**< array of mutexes (allocated together with the structure)*/
} reference_counter_group_t;

/** Initializes reference counter - sets its value to 1. 
 * After call to this function, the caller is owner of first 
 * reference. From now it can call other functions like
 * \ref add_reference or \ref remove_reference. 
 *
 * This function initializes the mutex - it chooses one from 
 * group mutexes. The mutex can not be changed after this 
 * call (it is only for reading). */
void init_reference_counter(reference_counter_group_t *grp, reference_counter_data_t *ref);

/** Adds reference - increments reference counter.
 * This function can be called only by owner of at least one reference! */
void add_reference(reference_counter_data_t *ref);

/** Returns the value of reference counter. This function is mostly
 * useless. */
int get_reference_count(reference_counter_data_t *ref);

/** Removes reference - decrements reference counter.
 * This function can be called only by owner of at least one reference!
 *
 * \retval 0 if reference removed, but other references exist
 * \retval 1 if removed last reference
 *  */
int remove_reference(reference_counter_data_t *ref);

/** Creates and initializes group of reference counters. All reference 
 * counters 'belonging' to this group are using the same set of mutexes. */
reference_counter_group_t *create_reference_counter_group(int mutex_cnt);

/** Destroys all resources used by reference counter group.
 * After this function call no reference counter initialized
 * by this group can be used. */
void free_reference_counter_group(reference_counter_group_t *grp);

/** @} 
 * @} */

#ifdef __cplusplus
}
#endif

#endif
