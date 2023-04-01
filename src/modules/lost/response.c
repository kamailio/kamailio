/*
 * lost module LoST response parsing functions
 *
 * Copyright (C) 2022 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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
 */

/*!
 * \file
 * \brief Kamailio lost :: response
 * \ingroup lost
 * Module: \ref lost
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "../../core/parser/msg_parser.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_ppi_pai.h"
#include "../../core/dprint.h"
#include "../../core/mem/mem.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/rand/kam_rand.h"

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "pidf.h"
#include "utilities.h"
#include "response.h"

/* 
 * is_http_laquot(search)
 * return 1 if true else 0
 */
int is_http_laquot(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("<http:")) {
		return 0;
	}
	if((*(search + 0) == '<')
			&& ((*(search + 1) == 'h') || (*(search + 1) == 'H'))
			&& ((*(search + 2) == 't') || (*(search + 2) == 'T'))
			&& ((*(search + 3) == 't') || (*(search + 3) == 'T'))
			&& ((*(search + 4) == 'p') || (*(search + 4) == 'P'))
			&& ((*(search + 5) == ':'))) {
		return 1;
	}
	return 0;
}

/* 
 * is_https_laquot(search)
 * return 1 if true else 0
 */
int is_https_laquot(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("<https:")) {
		return 0;
	}
	if((*(search + 0) == '<')
			&& ((*(search + 1) == 'h') || (*(search + 1) == 'H'))
			&& ((*(search + 2) == 't') || (*(search + 2) == 'T'))
			&& ((*(search + 3) == 't') || (*(search + 3) == 'T'))
			&& ((*(search + 4) == 'p') || (*(search + 4) == 'P'))
			&& ((*(search + 5) == 's') || (*(search + 5) == 'S'))
			&& ((*(search + 6) == ':'))) {
		return 1;
	}
	return 0;
}

/* 
 * is_http(search)
 * return 1 if true else 0
 */
int is_http(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("http:")) {
		return 0;
	}
	if(((*(search + 0) == 'h') || (*(search + 0) == 'H'))
			&& ((*(search + 1) == 't') || (*(search + 1) == 'T'))
			&& ((*(search + 2) == 't') || (*(search + 2) == 'T'))
			&& ((*(search + 3) == 'p') || (*(search + 3) == 'P'))
			&& ((*(search + 4) == ':'))) {
		return 1;
	}
	return 0;
}

/* 
 * is_https(search)
 * return 1 if true else 0
 */
int is_https(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("https:")) {
		return 0;
	}
	if(((*(search + 0) == 'h') || (*(search + 0) == 'H'))
			&& ((*(search + 1) == 't') || (*(search + 1) == 'T'))
			&& ((*(search + 2) == 't') || (*(search + 2) == 'T'))
			&& ((*(search + 3) == 'p') || (*(search + 3) == 'P'))
			&& ((*(search + 4) == 's') || (*(search + 4) == 'S'))
			&& ((*(search + 5) == ':'))) {
		return 1;
	}
	return 0;
}

/* 
 * is_cid_laquot(search)
 * return 1 if true else 0
 */
int is_cid_laquot(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("<cid:")) {
		return 0;
	}
	if((*(search + 0) == '<')
			&& ((*(search + 1) == 'c') || (*(search + 1) == 'C'))
			&& ((*(search + 2) == 'i') || (*(search + 2) == 'I'))
			&& ((*(search + 3) == 'd') || (*(search + 3) == 'D'))
			&& (*(search + 4) == ':')) {
		return 1;
	}
	return 0;
}

/* 
 * is_cid(search)
 * return 1 if true else 0
 */
int is_cid(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("cid:")) {
		return 0;
	}
	if(((*(search + 0) == 'c') || (*(search + 0) == 'C'))
			&& ((*(search + 1) == 'i') || (*(search + 1) == 'I'))
			&& ((*(search + 2) == 'd') || (*(search + 2) == 'D'))
			&& (*(search + 3) == ':')) {
		return 1;
	}
	return 0;
}

/* 
 * is_urn(search)
 * return 1 if true else 0
 */
int is_urn(char *search)
{
	if(search == NULL) {
		return 0;
	}
	if(strlen(search) < strlen("urn:")) {
		return 0;
	}
	if(((*(search + 0) == 'u') || (*(search + 0) == 'U'))
			&& ((*(search + 1) == 'r') || (*(search + 1) == 'R'))
			&& ((*(search + 2) == 'n') || (*(search + 2) == 'N'))
			&& (*(search + 3) == ':')) {
		return 1;
	}
	return 0;
}

/*
 * lost_new_response(void)
 * creates a new response object in private memory and returns a pointer
 */
p_lost_fsr_t lost_new_response(void)
{
	p_lost_fsr_t res;

	res = (p_lost_fsr_t)pkg_malloc(sizeof(s_lost_fsr_t));
	if(res == NULL) {
		return NULL;
	}
	res->category = OTHER;
	res->mapping = NULL;
	res->path = NULL;
	res->warnings = NULL;
	res->errors = NULL;
	res->redirect = NULL;
	res->uri = NULL;

	LM_DBG("### reponse data initialized\n");

	return res;
}

/*
 * lost_new_response_type(void)
 * creates a new response type object in private memory and returns a pointer
 */
p_lost_type_t lost_new_response_type(void)
{
	p_lost_type_t res;

	res = (p_lost_type_t)pkg_malloc(sizeof(s_lost_type_t));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	res->type = NULL;
	res->target = NULL;
	res->source = NULL;

	res->info = (p_lost_info_t)pkg_malloc(sizeof(s_lost_info_t));
	if(res->info == NULL) {
		PKG_MEM_ERROR;
	} else {
		res->info->text = NULL;
		res->info->lang = NULL;
	}

	LM_DBG("### type data initialized\n");

	return res;
}

/*
 * lost_new_response_issues(void)
 * creates a new issues object in private memory and returns a pointer
 */
p_lost_issue_t lost_new_response_issues(void)
{
	p_lost_issue_t res = NULL;

	res = (p_lost_issue_t)pkg_malloc(sizeof(s_lost_issue_t));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	res->issue = NULL;
	res->next = NULL;

	LM_DBG("### issues data initialized\n");

	return res;
}

/*
 * lost_new_response_data(void)
 * creates a new response data object in private memory and returns a pointer
 */
p_lost_data_t lost_new_response_data(void)
{
	p_lost_data_t res;

	res = (p_lost_data_t)pkg_malloc(sizeof(s_lost_data_t));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}
	res->expires = NULL;
	res->updated = NULL;
	res->source = NULL;
	res->sourceid = NULL;
	res->urn = NULL;
	res->name = NULL;
	res->number = NULL;

	LM_DBG("### mapping data initialized\n");

	return res;
}

/*
 * lost_new_response_list(void)
 * creates a new response list object in private memory and returns a pointer
 */
p_lost_list_t lost_new_response_list(void)
{
	p_lost_list_t list;

	list = (p_lost_list_t)pkg_malloc(sizeof(s_lost_list_t));
	if(list == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	}

	list->value = NULL;
	list->next = NULL;

	LM_DBG("### list data initialized\n");

	return list;
}

/*
 * lost_reverse_response_list(list)
 * reverses list order of a list object
 */
void lost_reverse_response_list(p_lost_list_t *head)
{
	p_lost_list_t prev = NULL;
	p_lost_list_t next = NULL;
	p_lost_list_t current = *head;

	while(current != NULL) {
		next = current->next;
		current->next = prev;
		prev = current;
		current = next;
	}
	*head = prev;
}

/*
 * lost_append_response_list(list, str)
 * appends str value to list object and returns str len
 */
int lost_append_response_list(p_lost_list_t *head, str val)
{
	int len = 0;
	p_lost_list_t new = NULL;
	p_lost_list_t current = *head;

	new = lost_new_response_list();
	if (new != NULL) {
		new->value = lost_copy_string(val, &len);
		new->next = NULL;

		LM_DBG("### new list data [%.*s]\n", val.len, val.s);

		if (current == NULL) {
			*head = new;
			return len;
		}
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = new;
	}
	return len;
}

/*
 * lost_search_response_list(list, value, search)
 * looks for search string in list object and returns pointer if found
 */
int lost_search_response_list(p_lost_list_t *list, char **val, const char *str)
{
	p_lost_list_t cur;
	p_lost_list_t next;

	if(*list == NULL)
		return 0;

	if(str == NULL)
		return 0;

	LM_DBG("### list data search [%s]\n", str);

	next = *list;
	while((cur = next) != NULL) {
		next = cur->next;
		if(cur->value != NULL) {
			if(strncasecmp(cur->value, str, strlen(str)) == 0) {
				*val = cur->value;

				LM_DBG("###\t[%s] found\n", cur->value);
				
				return 1;
			}
		}
	}

	return 0;
}

/*
 * lost_delete_response_list(list)
 * removes response list from private memory
 */
void lost_delete_response_list(p_lost_list_t *list)
{
	p_lost_list_t cur;

	if(*list == NULL)
		return;

	while((cur = *list) != NULL) {
		*list = cur->next;
		if(cur->value != NULL) {
			pkg_free(cur->value);
		}
		pkg_free(cur);
	}

	*list = NULL;

	LM_DBG("### list data deleted\n");

	return;
}

/*
 * lost_delete_response_msg(msg)
 * removes response info from private memory
 */
void lost_delete_response_info(p_lost_info_t *info)
{
	p_lost_info_t ptr;

	if(*info == NULL)
		return;

	ptr = *info;

	if(ptr->text != NULL) {
		pkg_free(ptr->text);
	}
	if(ptr->lang != NULL) {
		pkg_free(ptr->lang);
	}

	pkg_free(ptr);
	*info = NULL;

	LM_DBG("### info data deleted\n");

	return;
}

/*
 * lost_delete_response_msg(type)
 * removes response type from private memory
 */
void lost_delete_response_type(p_lost_type_t *type)
{
	p_lost_type_t ptr;

	if(*type == NULL)
		return;

	ptr = *type;

	if(ptr->type != NULL) {
		pkg_free(ptr->type);
	}
	if(ptr->target != NULL) {
		pkg_free(ptr->target);
	}
	if(ptr->source != NULL) {
		pkg_free(ptr->source);
	}
	if(ptr->info != NULL) {
		lost_delete_response_info(&ptr->info);
	}

	pkg_free(ptr);
	*type = NULL;

	LM_DBG("### type data deleted\n");

	return;
}

/*
 * lost_delete_response_issue(list)
 * removes response issue object from private memory
 */
void lost_delete_response_issues(p_lost_issue_t *list)
{
	p_lost_issue_t cur;

	while((cur = *list) != NULL) {
		*list = cur->next;
		if(cur->issue != NULL) {
			lost_delete_response_type(&cur->issue);
		}
		pkg_free(cur);
	}

	*list = NULL;

	LM_DBG("### issue data deleted\n");

	return;
}

/*
 * lost_delete_response_issue(mapping)
 * removes respone data object from private memory
 */
void lost_delete_response_data(p_lost_data_t *m)
{
	p_lost_data_t ptr;

	if(*m == NULL)
		return;

	ptr = *m;

	if(ptr->expires != NULL) {
		pkg_free(ptr->expires);
	}
	if(ptr->updated != NULL) {
		pkg_free(ptr->updated);
	}
	if(ptr->source != NULL) {
		pkg_free(ptr->source);
	}
	if(ptr->sourceid != NULL) {
		pkg_free(ptr->sourceid);
	}
	if(ptr->urn != NULL) {
		pkg_free(ptr->urn);
	}
	if(ptr->name != NULL) {
		lost_delete_response_info(&ptr->name);
	}
	if(ptr->number != NULL) {
		pkg_free(ptr->number);
	}

	pkg_free(ptr);
	*m = NULL;

	LM_DBG("### mapping data deleted\n");

	return;
}

/*
 * lost_free_findServiceResponse(response)
 * removes findServiceResponse object from private memory
 */
void lost_free_findServiceResponse(p_lost_fsr_t *res)
{
	p_lost_fsr_t ptr;

	if(*res == NULL)
		return;

	ptr = *res;

	if(ptr->mapping != NULL) {
		lost_delete_response_data(&ptr->mapping);
	}
	if(ptr->path != NULL) {
		lost_delete_response_list(&ptr->path);
	}
	if(ptr->warnings != NULL) {
		lost_delete_response_issues(&ptr->warnings);
	}
	if(ptr->errors != NULL) {
		lost_delete_response_issues(&ptr->errors);
	}
	if(ptr->redirect != NULL) {
		lost_delete_response_type(&ptr->redirect);
	}
	if(ptr->uri != NULL) {
		lost_delete_response_list(&ptr->uri);
	}

	pkg_free(ptr);
	*res = NULL;

	LM_DBG("### findServiceResponse deleted\n");

	return;
}

/*
 * lost_get_response_issue(node)
 * parses response issue (errors, warnings) and writes 
 * results to issue object
 */
p_lost_issue_t lost_get_response_issues(xmlNodePtr node)
{
	xmlNodePtr cur = NULL;

	p_lost_issue_t list = NULL;
	p_lost_issue_t new = NULL;
	p_lost_type_t issue = NULL;

	str tmp = STR_NULL;

	int len = 0;

	if(node == NULL) {
		return NULL;
	}

	LM_DBG("### LOST\t%s\n", node->name);

	cur = node->children;
	while(cur) {
		if(cur->type == XML_ELEMENT_NODE) {
			/* get a new response type object */
			issue = lost_new_response_type();
			if(issue == NULL) {
				/* didn't get it ... return */
				break;
			}
			/* get issue type */
			tmp.s = (char *)cur->name;
			tmp.len = strlen((char *)cur->name);
			/* copy issue type to object */
			len = 0;
			if(tmp.len > 0 && tmp.s != NULL) {
				issue->type = lost_copy_string(tmp, &len);
			}
			if(len == 0) {
				/* issue type not found, clean up and return */
				lost_delete_response_type(&issue); /* clean up */
				break;
			}
			/* parse source property */
			len = 0;
			issue->source = lost_get_property(cur->parent, MAPP_PROP_SRC, &len);
			if(len == 0) {
				/* source property not found, clean up and return */
				lost_delete_response_type(&issue); /* clean up */
				break;
			}			

			LM_DBG("###\t[%s]\n", issue->type);

			/* type and source property found ... parse text and copy */ 
			if(issue->info != NULL) {
				issue->info->text = lost_get_property(cur, PROP_MSG, &len);
				issue->info->lang = lost_get_property(cur, PROP_LANG, &len);
			}
			/* get a new list element */
			new = lost_new_response_issues();
			if(new == NULL) {
				/* didn't get it, clean up and return */
				lost_delete_response_type(&issue); /* clean up */
				break;
			}
			/* parsing done, append object to list */
			new->issue = issue;
			new->next = list;
			list = new;

			/* get next element */
			cur = cur->next;
		}
	}

	return list;
}

/*
 * lost_get_response_list(node, name, property)
 * parses response list and writes results to list object
 */
p_lost_list_t lost_get_response_list(
		xmlNodePtr node, const char *name, const char *prop)
{
	xmlNodePtr cur = NULL;

	p_lost_list_t list = NULL;
	p_lost_list_t new = NULL;

	str tmp = STR_NULL;
	int len = 0;

	if(node == NULL) {
		return list;
	}

	LM_DBG("### LOST\t%s\n", node->name);

	for(cur = node; cur; cur = cur->next) {
		if(cur->type == XML_ELEMENT_NODE) {
			if(!xmlStrcasecmp(cur->name, (unsigned char *)name)) {
				new = lost_new_response_list();
				if(new != NULL) {
					if(prop) {
						tmp.s = lost_get_property(cur, prop, &tmp.len);
					} else {
						tmp.s = lost_get_content(cur, name, &tmp.len);
					}
					if(tmp.len > 0 && tmp.s != NULL) {
						new->value = lost_copy_string(tmp, &len);

						LM_DBG("###\t[%s]\n", new->value);

						new->next = list;
						list = new;
						lost_free_string(&tmp); /* clean up */
					} else {
						lost_delete_response_list(&new); /* clean up */
					}
				}
			}
		} else {
			/* not an uri element */
			break;
		}
	}

	return list;
}

/*
 * lost_get_response_element(node, name)
 * parses response element and returns a char pointer
 */
char *lost_get_response_element(xmlNodePtr node, const char *name)
{
	char *ret = NULL;
	int len = 0;

	if(node == NULL) {
		return ret;
	}

	LM_DBG("### LOST %s\n", node->name);

	ret = lost_get_content(node, name, &len);

	LM_DBG("###\t[%.*s]\n", len, ret);

	return ret;
}

/*
 * lost_get_response_type(node, name)
 * parses response type and writes results to type object
 */
p_lost_type_t lost_get_response_type(xmlNodePtr node, const char *name)
{
	p_lost_type_t res = NULL;

	str tmp = STR_NULL;

	int len = 0;

	if(node == NULL) {
		return res;
	}

	LM_DBG("### LOST %s\n", node->name);

	tmp.s = lost_get_childname(node, name, &tmp.len);
	if(tmp.len > 0 && tmp.s != NULL) {
		res = lost_new_response_type();
		if(res != NULL) {
			res->type = lost_copy_string(tmp, &len);
			if(len > 0) {

				LM_DBG("###\t[%s]\n", res->type);
			}
			if(res->info != NULL) {
				res->info->text =
						lost_get_property(node->children, PROP_MSG, &len);
				res->info->lang =
						lost_get_property(node->children, PROP_LANG, &len);
			}
		}
		lost_free_string(&tmp); /* clean up */
	}

	return res;
}

/*
 * lost_get_response_info(node, name, property)
 * parses response info (text, language) and writes results to info object
 */
p_lost_info_t lost_get_response_info(
		xmlNodePtr node, const char *name, const char *prop)
{
	p_lost_info_t res = NULL;

	str tmp = STR_NULL;

	int len = 0;

	if(node == NULL) {
		return res;
	}

	LM_DBG("### LOST %s\n", node->name);

	res = (p_lost_info_t)pkg_malloc(sizeof(s_lost_info_t));
	if(res == NULL) {
		PKG_MEM_ERROR;
		return NULL;
	} else {
		res->text = NULL;
		res->lang = NULL;
		if(prop) {
			tmp.s = lost_get_property(node, PROP_MSG, &tmp.len);
		} else {
			tmp.s = lost_get_content(node, name, &tmp.len);
		}
		if(tmp.len > 0 && tmp.s != NULL) {
			res->text = lost_copy_string(tmp, &len);
			if(len > 0) {

				LM_DBG("###\t\t[%s]\n", res->text);
			}
			lost_free_string(&tmp); /* clean up */
		}
		res->lang = lost_get_property(node, PROP_LANG, &len);

		LM_DBG("###\t\t[%s]\n", res->lang);
	}

	return res;
}

/*
 * lost_print_findServiceResponse(response)
 * prints/logs response elements
 */
void lost_print_findServiceResponse(p_lost_fsr_t res)
{
	if(res == NULL) {
		return;
	}
	p_lost_data_t m = NULL;
	p_lost_type_t r = NULL;
	p_lost_issue_t e = NULL;
	p_lost_issue_t w = NULL;
	p_lost_type_t t = NULL;
	p_lost_list_t list = NULL;

	switch(res->category) {
		case RESPONSE:
			if((m = res->mapping) != NULL) {
				LM_INFO("### LOST %s:\t[%s]\n", MAPP_PROP_EXP, m->expires);
				LM_INFO("### LOST %s:\t[%s]\n", MAPP_PROP_LUP, m->updated);
				LM_INFO("### LOST %s:\t[%s]\n", MAPP_PROP_SRC, m->source);
				LM_INFO("### LOST %s:\t[%s]\n", MAPP_PROP_SID, m->sourceid);
				if(m->name) {
					LM_INFO("### LOST %s:\t[%s (%s)]\n", DISPNAME_NODE,
							m->name->text, m->name->lang);
				}
				LM_INFO("### LOST %s:\t[%s]\n", SERVICENR_NODE, m->number);
				LM_INFO("### LOST %s:\t[%s]\n", SERVICE_NODE, m->urn);
				if((list = res->uri) != NULL) {
					while(list) {
						LM_INFO("### LOST %s:\t[%s]\n", MAPP_NODE_URI,
								list->value);
						list = list->next;
					}
				}
				if((list = res->path) != NULL) {
					while(list) {
						LM_INFO("### LOST %s:\t[%s]\n", PATH_NODE_VIA,
								list->value);
						list = list->next;
					}
				}
				if((w = res->warnings) != NULL) {
					while(w) {
						t = w->issue;
						LM_INFO("###! LOST %s reported ...\n", t->source);
						LM_INFO("###! LOST %s:\n", t->type);
						if(t->info) {
							LM_INFO("###! LOST\t[%s (%s)]\n", t->info->text,
									t->info->lang);
						}
						w = w->next;
					}
				}
			}
			break;
		case REDIRECT:
			if((r = res->redirect) != NULL) {
				LM_INFO("### LOST %s:\t[%s]\n", RED_PROP_TAR, r->target);
				LM_INFO("### LOST %s:\t[%s]\n", RED_PROP_SRC, r->source);
				if(r->info) {
					LM_INFO("### LOST %s:\t[%s (%s)]\n", PROP_MSG,
							r->info->text, r->info->lang);
				}
			}
			break;
		case ERROR:
			if((e = res->errors) != NULL) {
				while(e) {
					t = e->issue;
					LM_INFO("###! LOST %s reported ...\n", t->source);
					LM_INFO("###! LOST %s:\n", t->type);
					if(t->info) {
						LM_INFO("###! LOST\t[%s (%s)]\n", t->info->text,
								t->info->lang);
					}
					e = e->next;
				}
			}
			break;
		case OTHER:
		default:
			break;
	}

	return;
}

/*
 * lost_parse_findServiceResponse(str)
 * read and parse the findServiceResponse xml string
 */
p_lost_fsr_t lost_parse_findServiceResponse(str ret)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr node = NULL;

	p_lost_fsr_t res = NULL;
	p_lost_data_t m = NULL;
	p_lost_type_t r = NULL;

	int len = 0;

	doc = xmlReadMemory(ret.s, ret.len, 0, 0,
			XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_NOCDATA);

	if(doc == NULL) {
		LM_ERR("invalid xml document: [%.*s]\n", ret.len, ret.s);
		doc = xmlRecoverMemory(ret.s, ret.len);
		if(doc == NULL) {
			LM_ERR("xml document recovery failed on: [%.*s]\n", ret.len, ret.s);
			return NULL;
		}

		LM_DBG("xml document recovered\n");
	}
	root = xmlDocGetRootElement(doc);
	if(root == NULL) {
		LM_ERR("empty xml document: [%.*s]\n", ret.len, ret.s);
		xmlFreeDoc(doc); /* clean up */
		return NULL;
	}

	LM_DBG("### %s received\n", root->name);

	res = lost_new_response();
	if(res == NULL) {
		PKG_MEM_ERROR;
		xmlFreeDoc(doc); /* clean up */
		return NULL;
	}
	/* check the root element ... */
	/* findServiceResponse */
	if(!xmlStrcmp(root->name, (unsigned char *)ROOT_NODE)) {
		m = lost_new_response_data();
		if(m == NULL) {
			PKG_MEM_ERROR;
			/* clean up */
			xmlFreeDoc(doc);
			lost_free_findServiceResponse(&res);
			return NULL;
		}
		/* get mapping properties */
		node = xmlDocGetNodeByName(doc, MAPP_NODE, NULL);
		m->expires = lost_get_property(node, MAPP_PROP_EXP, &len);
		m->updated = lost_get_property(node, MAPP_PROP_LUP, &len);
		m->source = lost_get_property(node, MAPP_PROP_SRC, &len);
		m->sourceid = lost_get_property(node, MAPP_PROP_SID, &len);
		node = NULL;
		/* get the displayName element */
		node = xmlDocGetNodeByName(doc, DISPNAME_NODE, NULL);
		m->name = lost_get_response_info(node, DISPNAME_NODE, NULL);
		/* get the serviceNumber element */
		m->number = lost_get_response_element(root, SERVICENR_NODE);
		/* get the service element */
		m->urn = lost_get_response_element(root, SERVICE_NODE);
		node = NULL;
		/* get the via path (list) */
		node = xmlDocGetNodeByName(doc, PATH_NODE_VIA, NULL);
		res->path = lost_get_response_list(node, PATH_NODE_VIA, MAPP_PROP_SRC);
		lost_reverse_response_list(&res->path);
		node = NULL;
		/* get the uri element list */
		node = xmlDocGetNodeByName(doc, MAPP_NODE_URI, NULL);
		res->uri = lost_get_response_list(node, MAPP_NODE_URI, NULL);
		lost_reverse_response_list(&res->uri);
		node = NULL;
		/* get the warnings element */
		node = xmlDocGetNodeByName(doc, WARNINGS_NODE, NULL);
		res->warnings = lost_get_response_issues(node);
		/* return */
		res->mapping = m;
		res->category = RESPONSE;
		/* check the root element ... */
		/* errors */
	} else if(!xmlStrcmp(root->name, (unsigned char *)ERRORS_NODE)) {
		res->errors = lost_get_response_issues(root);
		/* return */
		res->category = ERROR;
		/* check the root element ... */
		/* redirect */
	} else if(!xmlStrcmp(root->name, (unsigned char *)RED_NODE)) {
		r = lost_new_response_type();
		if(r == NULL) {
			PKG_MEM_ERROR;
			xmlFreeDoc(doc); /* clean up */
			lost_free_findServiceResponse(&res);
			return NULL;
		}
		r->source = lost_get_property(root, RED_PROP_SRC, &len);
		r->target = lost_get_property(root, RED_PROP_TAR, &len);
		if(r->info != NULL) {
			r->info->text = lost_get_property(root, RED_PROP_MSG, &len);
			r->info->lang = lost_get_property(root, PROP_LANG, &len);
		}
		/* return */
		res->redirect = r;
		res->category = REDIRECT;
		/* check the root element ... */
		/* unknown */
	} else {
		LM_ERR("root element is not valid: [%.*s]\n", ret.len, ret.s);
		res->category = OTHER;
	}

	xmlFreeDoc(doc); /* clean up */

	return res;
}

/*
 * lost_check_HeldResponse(node)
 * does a quick check of HELD dereference response and returns ...
 * 0: neither location value nor reference found
 * 1: location reference found
 * 2: location value found
 * 3: location value and reference found
 * multiple occurences are ignored
 */
int lost_check_HeldResponse(xmlNodePtr node)
{
	char *tmp = NULL;

	int ret = 0; /* response error */

	tmp = xmlNodeGetNodeContentByName(node, "location-info", NULL);
	if(tmp != NULL) {
		ret += HELD_RESPONSE_VALUE; /* LocByVal: civic or geodetic */
	}
	xmlFree(tmp); /* clean up */

	tmp = xmlNodeGetNodeContentByName(node, "locationURI", NULL);
	if(tmp != NULL) {
		ret += HELD_RESPONSE_REFERENCE; /* LocByRef: reference */
	}
	xmlFree(tmp); /* clean up */

	return ret;
}