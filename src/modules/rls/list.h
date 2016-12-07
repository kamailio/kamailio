#ifndef _LIST_H
#define _LIST_H

#include <string.h>
#include "../../dprint.h"
#include "../../mem/mem.h"

typedef struct list_entry
{
	str *strng;
	struct list_entry *next;
} list_entry_t;

static inline list_entry_t *list_insert(str *strng, list_entry_t *list, int *duplicate)
{
	int cmp;
	list_entry_t *p, *q;

	if (duplicate != NULL)
		*duplicate = 0;

	if (strng == NULL || strng->s == NULL || strng->len == 0)
	{
		LM_ERR("bad string\n");
		return list;
	}

	if ((p = (list_entry_t *) pkg_malloc(sizeof(list_entry_t))) == NULL)
	{
		LM_ERR("out of memory\n");
		return list;
	}
	p->strng = strng;
	p->next = NULL;

	if (list == NULL)
		return p;

	cmp = strncmp(list->strng->s, strng->s, strng->len);

	if (cmp == 0)
	{
		if (duplicate != NULL)
		{
			*duplicate = 1;
			pkg_free(p);
			return list;
		}
	}
	if (cmp > 0)
	{
		p->next = list;
		return p;
	}
	else
	{
		q = list;
		while (q->next != NULL && (cmp = strncmp(q->next->strng->s, strng->s, strng->len)) < 0)
			q = q->next;

		if (cmp == 0)
		{
			if (duplicate != NULL)
			{
				*duplicate = 1;
				pkg_free(p);
				return list;
			}
		}

		p->next = q->next;
		q->next = p;
		return list;
	}
}

static inline list_entry_t *list_remove(str strng, list_entry_t *list)
{
	int cmp = 0;
	list_entry_t *p = list;

	if (list != NULL)
	{
		if (strncmp(p->strng->s, strng.s, strng.len) == 0)
		{
			pkg_free(p->strng->s);
			pkg_free(p->strng);
			pkg_free(p);
			return list->next;
		}
		else
		{
			list_entry_t *p = list, *q;

			while (p->next != NULL && (cmp = strncmp(p->next->strng->s, strng.s, strng.len)) < 0)
				p = p->next;

			if (cmp == 0)
			{
				q = p->next;
				p->next = q->next;
				pkg_free(q->strng->s);
				pkg_free(q->strng);
				pkg_free(q);
			}
		}
	}
	return list;
}

static inline str *list_pop(list_entry_t **list)
{
	str *ret = NULL;
	list_entry_t *tmp;

	if (*list != NULL)
	{
		ret = (*list)->strng;

		if ((*list)->next == NULL)
		{
			pkg_free(*list);
			*list = NULL;
		}
		else
		{
			tmp = *list;
			*list = (*list)->next;
			pkg_free(tmp);
		}
	}

	return ret;
}

static inline void list_free(list_entry_t **list)
{
	str *strng;

	while ((strng = list_pop(list)) != NULL)
	{
		pkg_free(strng->s);
		pkg_free(strng);
	}
	*list = NULL;
}

#endif /* _LIST_H */
