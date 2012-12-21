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
#ifndef _MAPPING_H
#define _MAPPING_H

typedef struct {
	char *protocol;
	char *username;
	char *password;
} extern_account_t;

typedef struct {
	char *protocol;
	char *username;
} extern_user_t;

char *find_sip_user(char *extern_user);
extern_account_t *find_accounts(char* sip_user, int* count);
extern_user_t *find_users(char* sip_user, int* count);

void extern_account_free(extern_account_t *accounts, int count);
void extern_user_free(extern_user_t *users, int count);

#endif
