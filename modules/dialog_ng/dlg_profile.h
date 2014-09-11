/*
 * $Id$
 *
 * Copyright (C) 2008 Voice System SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 * 2008-04-20  initial version (bogdan)
 *
 */



#ifndef _DIALOG_DLG_PROFILE_H_
#define _DIALOG_DLG_PROFILE_H_

#include "../../parser/msg_parser.h"
#include "../../locking.h"
#include "../../str.h"
#include "../../modules/tm/h_table.h"



/*!
 * \file
 * \brief Profile related functions for the dialog module
 * \ingroup dialog
 * Module: \ref dialog
 */


/*! dialog profile hash list */
struct dlg_profile_hash {
	str value; /*!< hash value */
	struct dlg_cell *dlg; /*!< dialog cell */
	struct dlg_profile_hash *next;
	struct dlg_profile_hash *prev;
	unsigned int hash; /*!< position in the hash table */
};


/*! list with links to dialog profiles */
struct dlg_profile_link {
	struct dlg_profile_hash hash_linker;
	struct dlg_profile_link  *next;
	struct dlg_profile_table *profile;
};


/*! dialog profile entry */
struct dlg_profile_entry {
	struct dlg_profile_hash *first;
	unsigned int content; /*!< content of the entry */
};


/*! dialog profile table */
struct dlg_profile_table {
	str name; /*!< name of the dialog profile */
	unsigned int size; /*!< size of the dialog profile */
	unsigned int has_value; /*!< 0 for profiles without value, otherwise it has a value */
	gen_lock_t lock; /*! lock for concurrent access */
	struct dlg_profile_entry *entries;
	struct dlg_profile_table *next;
};


struct dlg_cell *get_current_dlg_pointer(void);

void reset_current_dlg_pointer(void);

struct dlg_cell* get_dialog_from_tm(struct cell *t);

struct dlg_cell *get_current_dialog(struct sip_msg *msg);

/*!
 * \brief Add profile definitions to the global list
 * \see new_dlg_profile
 * \param profiles profile name
 * \param has_value set to 0 for a profile without value, otherwise it has a value
 * \return 0 on success, -1 on failure
 */
int add_profile_definitions( char* profiles, unsigned int has_value);


/*!
 * \brief Destroy the global dialog profile list
 */
void destroy_dlg_profiles(void);


/*!
 * \brief Search a dialog profile in the global list
 * \note Linear search, this won't have the best performance for huge profile lists
 * \param name searched dialog profile
 * \return pointer to the profile on success, NULL otherwise
 */
struct dlg_profile_table* search_dlg_profile(str *name);


/*!
 * \brief Cleanup a profile
 * \param msg SIP message
 * \param flags unused
 * \param unused
 * \return 1
 */
int profile_cleanup( struct sip_msg *msg, unsigned int flags, void *param );


/*!
 * \brief Destroy dialog linkers
 * \param linker dialog linker
 */ 
void destroy_linkers(struct dlg_profile_link *linker);


/*!
 * \brief Set the global variables to the current dialog
 * \param msg SIP message
 * \param dlg dialog cell
 */
void set_current_dialog(struct sip_msg *msg, struct dlg_cell *dlg);


/*!
 * \brief Set the dialog profile
 * \param msg SIP message
 * \param value value
 * \param profile dialog profile table
 * \return 0 on success, -1 on failure
 */
int set_dlg_profile(struct sip_msg *msg, str *value,
		struct dlg_profile_table *profile);


/*!
 * \brief Unset a dialog profile
 * \param msg SIP message
 * \param value value
 * \param profile dialog profile table
 * \return 1 on success, -1 on failure
 */
int unset_dlg_profile(struct sip_msg *msg, str *value,
		struct dlg_profile_table *profile);


/*!
 * \brief Check if a dialog belongs to a profile
 * \param msg SIP message
 * \param profile dialog profile table
 * \param value value
 * \return 1 on success, -1 on failure
 */
int is_dlg_in_profile(struct sip_msg *msg, struct dlg_profile_table *profile,
		str *value);


/*!
 * \brief Get the size of a profile
 * \param profile evaluated profile
 * \param value value
 * \return the profile size
 */
unsigned int get_profile_size(struct dlg_profile_table *profile, str *value);


/*!
 * \brief Output a profile via MI interface
 * \param cmd_tree MI command tree
 * \param param MI parameter
 * \return MI root output on success, NULL on failure
 */
struct mi_root * mi_get_profile(struct mi_root *cmd_tree, void *param );


/*!
 * \brief List the profiles via MI interface
 * \param cmd_tree MI command tree
 * \param param unused
 * \return MI root output on success, NULL on failure
 */
struct mi_root * mi_profile_list(struct mi_root *cmd_tree, void *param );

/*!
 * \brief return true if the messages belongs to a tracked dialog
 */
int is_known_dlg(struct sip_msg *msg);

#endif
