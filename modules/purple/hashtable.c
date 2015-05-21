/* Kamailio PURPLE MODULE
 * 
 * Copyright (C) 2008 Atos Worldline
 * Contact: Eric PTAK <eric.ptak@atosorigin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdlib.h>
#include <glib.h>

#include "../../dprint.h"
#include "../../mem/mem.h"

#include "hashtable.h"

GHashTable *hash;

void hashtable_init(void) {
	hash = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
}

static int *get_counter(char *key) {
	int *d =  g_hash_table_lookup(hash, key);
	if (d == NULL) {
		gchar *k = g_strdup(key);
		d = (int*) pkg_malloc(sizeof(int));
		LM_DBG("counter created @0x%p\n", d);
		*d = 0;
		g_hash_table_insert(hash, k, d);
	}
	LM_DBG("counter@0x%p: key: %s ; value: %d\n", d, key, *d);
	return d;
}

static void remove_counter(char *key) {
	if (!g_hash_table_remove(hash, key))
		LM_ERR("error removing counter\n");

}

int hashtable_get_counter(char* key) {
	int *d = get_counter(key);
	return *d;
}

int hashtable_inc_counter(char* key) {
	LM_DBG("incrementing counter for <%s>\n", key);
	int *d = get_counter(key);
	*d = *d + 1;
	return *d;
}

int hashtable_dec_counter(char* key) {
	LM_DBG("decrementing counter for <%s>\n", key);
	int *d = get_counter(key);
	if (*d > 0)
		*d = *d - 1;
	if (*d == 0)
		remove_counter(key);
	return *d;
}

