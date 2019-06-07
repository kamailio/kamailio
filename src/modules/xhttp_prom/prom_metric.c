/*
 * Copyright (C) 2012 VoIP Embedded, Inc.
 *
 * Copyright (C) 2019 Vicente Hernando (Sonoc)
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

#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_param.h"

#include "prom_metric.h"
#include "prom.h"

/* TODO: Every internal function locks and unlocks the metric system. */

typedef enum metric_type {
	M_UNSET = 0,
	M_COUNTER = 1,
	M_GAUGE = 2
	/* TODO: Add more types. */
} metric_type_t;

/**
 * Struct to store a string (node of a list)
 */
typedef struct prom_lb_node_s {
	str n;
	struct prom_lb_node_s *next;
} prom_lb_node_t;

/**
 * Struct to store a list of strings (labels)
 */
typedef struct prom_lb_s {
	int n_elem; /* Number of strings. */
	struct prom_lb_node_s *lb;
	/* TODO: Hashes? */
} prom_lb_t;

/**
 * Struct to store a value of a label.
 */
typedef struct prom_lvalue_s {
	prom_lb_t lval;
	uint64_t ts; /* timespan. Last time metric was modified. */
	union {
		uint64_t cval;
		double gval;
	} m;
	struct prom_lvalue_s *next;
} prom_lvalue_t;

/**
 * Struct to store a metric.
 */
typedef struct prom_metric_s {
	metric_type_t type;
	str name;
	struct prom_lb_s *lb_name; /* Names of labels. */
	struct prom_lvalue_s *lval_list;
	struct prom_metric_s *next;
} prom_metric_t;

/**
 * Data related to Prometheus metrics.
 */
static prom_metric_t *prom_metric_list = NULL;
static gen_lock_t *prom_lock = NULL; /* Lock to protect Prometheus metrics. */
static uint64_t lvalue_timeout = 120000; /* Timeout in milliseconds for old lvalue struct. */

static void prom_counter_free(prom_metric_t *m_cnt);
static void prom_gauge_free(prom_metric_t *m_gg);
static void prom_metric_free(prom_metric_t *metric);
static void prom_lb_free(prom_lb_t *prom_lb, int shared_mem);
static void prom_lb_node_free(prom_lb_node_t *lb_node, int shared_mem);
static int prom_lb_node_add(prom_lb_t *m_lb, char *s, int len, int shared_mem);
static void prom_lvalue_free(prom_lvalue_t *plv);
static void prom_lvalue_list_free(prom_lvalue_t *plv);

/**
 * Free list of Prometheus metrics.
 */
static void prom_metric_list_free()
{
	prom_metric_t *p, *next;

	p = prom_metric_list;
	while (p) {
		next = p->next;
		prom_metric_free(p);
		p = next;
	}

	prom_metric_list = NULL;
}

/**
 * Initialize user defined metrics.
 */
int prom_metric_init(int timeout_minutes)
{
	/* Initialize timeout. minutes to milliseconds. */
	if (timeout_minutes < 1) {
		LM_ERR("Invalid timeout: %d\n", timeout_minutes);
		return -1;
	}
	lvalue_timeout = ((uint64_t)timeout_minutes) * 60000;
	LM_DBG("lvalue_timeout set to %" PRIu64 "\n", lvalue_timeout);
	
	/* Initialize lock. */
	prom_lock = lock_alloc();
	if (!prom_lock) {
		LM_ERR("Cannot allocate lock\n");
		return -1;
	}

	if (lock_init(prom_lock) == NULL) {
		LM_ERR("Cannot initialize the lock\n");
		lock_dealloc(prom_lock);
		prom_lock = NULL;
		return -1;
	}

	/* Everything went fine. */
	return 0;
}

/**
 * Close user defined metrics.
 */
void prom_metric_close()
{
	/* Free lock */
	if (prom_lock) {
		LM_DBG("Freeing lock\n");
		lock_destroy(prom_lock);
		lock_dealloc(prom_lock);
		prom_lock = NULL;
	}

	/* Free metric list. */
	if (prom_metric_list) {
		LM_DBG("Freeing list of Prometheus metrics\n");
		prom_metric_list_free();
	}
}

/**
 * Free a metric.
 */
static void prom_metric_free(prom_metric_t *metric)
{
	assert(metric);

	if (metric->type == M_COUNTER) {
		prom_counter_free(metric);
	} else if (metric->type == M_GAUGE) {
		prom_gauge_free(metric);
	} else {
		LM_ERR("Unknown metric: %d\n", metric->type);
		return;
	}
}

/**
 * Free a counter.
 */
static void prom_counter_free(prom_metric_t *m_cnt)
{
	assert(m_cnt);

	assert(m_cnt->type == M_COUNTER);

	if (m_cnt->name.s) {
		shm_free(m_cnt->name.s);
	}

	prom_lb_free(m_cnt->lb_name, 1);

	prom_lvalue_list_free(m_cnt->lval_list);
	
	shm_free(m_cnt);
}

/**
 * Get a metric based on its name.
 *
 * /return pointer to metric on success.
 * /return NULL on error.
 */
static prom_metric_t* prom_metric_get(str *s_name)
{
	prom_metric_t *p = prom_metric_list;

	while (p) {
		if (s_name->len == p->name.len && strncmp(s_name->s, p->name.s, s_name->len) == 0) {
			LM_DBG("Metric found: %.*s\n", p->name.len, p->name.s);
			break;
		}
		p = p->next;
	}

	return p;
}

/**
 * Compare prom_lb_t structure using some strings.
 *
 * /return 0 if prom_lb_t matches the strings.
 */
static int prom_lb_compare(prom_lb_t *plb, str *l1, str *l2, str *l3)
{
	if (plb == NULL) {
		if (l1 != NULL) {
			return -1;
		}
		return 0;
	}

	if (l1 == NULL) {
		if (plb->n_elem != 0) {
			return -1;
		}
		return 0;
	}

	/* At least one label. */
	prom_lb_node_t *p = plb->lb;	
	if (p == NULL) {
		return -1;
	}
	if (l1->len != p->n.len || strncmp(l1->s, p->n.s, l1->len)) {
		return -1;
	}

	p = p->next;
	if (l2 == NULL) {
		if (plb->n_elem != 1) {
			return -1;
		}
		return 0;
	}

	/* At least two labels. */
	if (p == NULL) {
		return -1;
	}
	if (l2->len != p->n.len || strncmp(l2->s, p->n.s, l2->len)) {
		return -1;
	}

	p = p->next;
	if (l3 == NULL) {
		if (plb->n_elem != 2) {
			return -1;
		}
		return 0;
	}

	/* At least three labels. */
	if (p == NULL) {
		return -1;
	}
	if (l3->len != p->n.len || strncmp(l3->s, p->n.s, l3->len)) {
		return -1;
	}

	return 0; 
}

/**
 * Compare two lval structures.
 *
 * /return 0 if they are the same.
 */
static int prom_lvalue_compare(prom_lvalue_t *p, str *l1, str *l2, str *l3)
{
	if (p == NULL) {
		LM_ERR("No lvalue structure\n");
		return -1;
	}

	if (prom_lb_compare(&p->lval, l1, l2, l3)) {
		return -1;
	}

	/* Comparison matches. */
	return 0;
}

/**
 * Free an lvalue structure.
 * Only defined for shared memory.
 */
static void prom_lvalue_free(prom_lvalue_t *plv)
{
	if (plv == NULL) {
		return;
	}

	/* Free list of strings. */
	prom_lb_node_t *lb_node = plv->lval.lb;
	while (lb_node) {
		prom_lb_node_t *next = lb_node->next;
		prom_lb_node_free(lb_node, 1);
		lb_node = next;
	}
	
	shm_free(plv);
}

/**
 * Free a list of lvalue structures.
 * Only defined for shared memory.
 */
static void prom_lvalue_list_free(prom_lvalue_t *plv)
{
	while (plv) {
		prom_lvalue_t *next = plv->next;
		prom_lvalue_free(plv);
		plv = next;
	}
}

/**
 * Fill lvalue data in prom_lvalue_t structure based on three strings.
 * Only defined for shared memory.
 *
 * /return 0 on success.
 */
static int prom_lvalue_lb_create(prom_lvalue_t *lv, str *l1, str *l2, str *l3)
{
	if (lv == NULL) {
		LM_ERR("No lvalue structure\n");
		return -1;
	}

	/* Initialize lv->lval */
	prom_lb_t *p = &lv->lval;
	p->n_elem = 0;
	p->lb = NULL;

	if (l1 == NULL) {
		/* No labels. */
		return 0;
	}

	/* At least 1 label. */
	if (prom_lb_node_add(p, l1->s, l1->len, 1)) {
		LM_ERR("Cannot add label string\n");
		return -1;
	}
	
	if (l2 == NULL) {
		return 0;
	}

	/* At least 2 labels. */
	if (prom_lb_node_add(p, l2->s, l2->len, 1)) {
		LM_ERR("Cannot add label string\n");
		return -1;
	}

	if (l3 == NULL) {
		return 0;
	}

	/* 3 labels. */
	if (prom_lb_node_add(p, l3->s, l3->len, 1)) {
		LM_ERR("Cannot add label string\n");
		return -1;
	}

	return 0;
}

/**
 * Create and insert a lvalue structure into a metric.
 * It only works in shared memory.
 *
 * /return pointer to newly created structure on success.
 * /return NULL on error.
 */
static prom_lvalue_t* prom_metric_lvalue_create(prom_metric_t *p_m, str *l1, str *l2, str *l3)
{
	if (p_m == NULL) {
		LM_ERR("No metric found\n");
		return NULL;
	}

	prom_lvalue_t *plv = NULL;
	plv = (prom_lvalue_t*)shm_malloc(sizeof(*plv));
	if (plv == NULL) {
		LM_ERR("shm out of memory\n");
		return NULL;
	}
	memset(plv, 0, sizeof(*plv));

	if (prom_lvalue_lb_create(plv, l1, l2, l3)) {
		LM_ERR("Cannot create list of strings\n");
		goto error;
	}

	/* Place plv at the end of lvalue list. */
	prom_lvalue_t **l = &p_m->lval_list;
	while (*l != NULL) {
		l = &((*l)->next);
	}
	*l = plv;
	plv->next = NULL;

	/* Everything went fine. */
	return plv;

error:
	prom_lvalue_free(plv);
	return NULL;
}

/**
 * Find a lvalue based on its labels.
 * If it does not exist it creates a new one and inserts it into the metric.
 *
 * /return pointer to lvalue on success.
 * /return NULL on error.
 */
static prom_lvalue_t* prom_lvalue_get_create(prom_metric_t *p_m, str *l1, str *l2, str *l3)
{
	if (!p_m) {
		LM_ERR("No metric found\n");
		return NULL;
	}

	/* Check number of elements in labels. */
	if (l1 == NULL) {
		if (p_m->lb_name != NULL) {
			LM_ERR("Number of labels does not match for metric: %.*s\n",
				   p_m->name.len, p_m->name.s);
			return NULL;
		}

	} else if (l2 == NULL) {
		if (!p_m || !p_m->lb_name || p_m->lb_name->n_elem != 1) {
			LM_ERR("Number of labels does not match for metric: %.*s\n",
				   p_m->name.len, p_m->name.s);
			return NULL;
		}

	} else if (l3 == NULL) {
		if (!p_m || !p_m->lb_name || p_m->lb_name->n_elem != 2) {
			LM_ERR("Number of labels does not match for metric: %.*s\n",
				   p_m->name.len, p_m->name.s);
			return NULL;
		}

	} else {
		if (!p_m || !p_m->lb_name || p_m->lb_name->n_elem != 3) {
			LM_ERR("Number of labels does not match for metric: %.*s\n",
				   p_m->name.len, p_m->name.s);
			return NULL;
		}

	} /* if l1 == NULL */

	/* Find existing prom_lvalue_t structure. */
	prom_lvalue_t *p = p_m->lval_list;
	while (p) {
		if (prom_lvalue_compare(p, l1, l2, l3) == 0) {
			LM_DBG("LValue structure found\n");
			return p;
		}
		p = p->next;
	}

	LM_DBG("Creating lvalue %.*s\n", p_m->name.len, p_m->name.s);
	/* No lvalue structure found. Create and insert a new one. */
	p = prom_metric_lvalue_create(p_m, l1, l2, l3);
	if (p == NULL) {
		LM_ERR("Cannot create a new lvalue structure\n");
		return NULL;
	}

	return p;
}

/**
 * Delete old lvalue structures in a metric.
 * Only for shared memory.
 */
static void prom_metric_timeout_delete(prom_metric_t *p_m)
{
	if (p_m == NULL) {
		return;
	}

	/* Get timestamp. */
	uint64_t ts;
	if (get_timestamp(&ts)) {
		LM_ERR("Fail to get timestamp\n");
		return;
	}

	/* Parse lvalue list deleting outdated items. */
	prom_lvalue_t **l = &p_m->lval_list;
	while (*l != NULL) {
		prom_lvalue_t *current = *l;
		
		if (ts - current->ts > lvalue_timeout) {
			LM_DBG("Timeout found\n");
			*l = (*l)->next;

			/* Free current lvalue. */
			prom_lvalue_free(current);

		} else {
			l = &((*l)->next);
		}
		
	} /* while *l != NULL */
}

/**
 * Delete old lvalue structures in list of metrics.
 */
static void	prom_metric_list_timeout_delete()
{
	prom_metric_t *p = prom_metric_list;

	while (p) {
		prom_metric_timeout_delete(p);
		p = p->next;
	}
}

/**
 * Get a lvalue based on its metric name and labels.
 * If metric name exists but no lvalue matches it creates a new lvalue.
 *
 * /return NULL if no lvalue was found or created.
 * /return pointer to lvalue on success.
 */
static prom_lvalue_t* prom_metric_lvalue_get(str *s_name, metric_type_t m_type,
											 str *l1, str *l2, str *l3)
{
	if (!s_name || s_name->len == 0 || s_name->s == NULL) {
		LM_ERR("No name for metric\n");
		return NULL;
	}

	/* Delete old lvalue structures. */
	prom_metric_list_timeout_delete();
	
    prom_metric_t *p_m = prom_metric_get(s_name);
	if (p_m == NULL) {
		LM_ERR("No metric found for name: %.*s\n", s_name->len, s_name->s);
		return NULL;
	}

	if (p_m->type != m_type) {
		LM_ERR("Metric type does not match for metric: %.*s\n", s_name->len, s_name->s);
		return NULL;
	}

	/* Get timestamp. */
	uint64_t ts;
	if (get_timestamp(&ts)) {
		LM_ERR("Fail to get timestamp\n");
		return NULL;
	}

	prom_lvalue_t *p_lv = NULL;
	p_lv = prom_lvalue_get_create(p_m, l1, l2, l3);
	if (p_lv == NULL) {
		LM_ERR("Failed to create lvalue\n");
		return NULL;
	}

	p_lv->ts = ts;
	LM_DBG("New timestamp: %" PRIu64 "\n", p_lv->ts);
	
	return p_lv;
}

/**
 * Free a node in a list of strings.
 */
static void prom_lb_node_free(prom_lb_node_t *lb_node, int shared_mem)
{
	if (lb_node == NULL) {
		return;
	}

	/* Free the str. */
	if (shared_mem) {
		if (lb_node->n.s) {
			shm_free(lb_node->n.s);
		}
	} else {
		if (lb_node->n.s) {
			pkg_free(lb_node->n.s);
		}	
	}
	
	if (shared_mem) {
		shm_free(lb_node);
	} else {
		pkg_free(lb_node);
	}

}

/**
 * Free a list of str (for labels).
 *
 * /param shared_mem 0 means pkg memory otherwise shared one.
 */
static void prom_lb_free(prom_lb_t *prom_lb, int shared_mem)
{
	if (prom_lb == NULL) {
		return;
	}

	/* Free nodes. */
	prom_lb_node_t *lb_node = prom_lb->lb;
	while (lb_node) {
		prom_lb_node_t *next = lb_node->next;
		prom_lb_node_free(lb_node, shared_mem);
		lb_node = next;
	}

	/* Free prom_bl_t object. */
	if (shared_mem) {
		shm_free(prom_lb);
	} else {
		pkg_free(prom_lb);
	}
}

#define LABEL_SEP ':'  /* Field separator for labels. */

/**
 * Add a string to list of strings.
 *
 * /param m_lb pointer to list of strings.
 * /param s whole string.
 * /param pos_start position of first character to add.
 * /param pos_end position after last character to add.
 *
 * /return 0 on success.
 */
static int prom_lb_node_add(prom_lb_t *m_lb, char *s, int len, int shared_mem)
{
	if (len <= 0) {
		LM_ERR("Void string to add\n");
		return -1;
	}
	
	LM_DBG("Label: adding (%.*s)\n", len, s);
	prom_lb_node_t *lb_node;
	if (shared_mem) {
		/* Shared memory */
		lb_node = (prom_lb_node_t*)shm_malloc(sizeof(*lb_node));
		if (lb_node == NULL) {
			LM_ERR("shm out of memory\n");
			goto error;
		}
		memset(lb_node, 0, sizeof(*lb_node));

	} else {
		/* Pkg memory */
		lb_node = (prom_lb_node_t*)pkg_malloc(sizeof(*lb_node));
		if (lb_node == NULL) {
			LM_ERR("pkg out of memory\n");
			goto error;
		}
		memset(lb_node, 0, sizeof(*lb_node));

	} /* if shared_mem */

	/* Allocate space for str. */
	if (shared_mem) {
		/* Shared memory */
		/* We left space for zero at the end. */
		lb_node->n.s = (char*)shm_malloc(len + 1);
		if (lb_node->n.s == NULL) {
			LM_ERR("shm out of memory\n");
			goto error;
		}
		memcpy(lb_node->n.s, s, len);
		lb_node->n.len = len;
		
	} else {
		/* Pkg memory */
		/* We left space for zero at the end. */
		lb_node->n.s = (char*)shm_malloc(len + 1);
		if (lb_node->n.s == NULL) {
			LM_ERR("shm out of memory\n");
			goto error;
		}
		memcpy(lb_node->n.s, s, len);
		lb_node->n.len = len;

	} /* if shared_mem */

	LM_DBG("Node str: %.*s\n", lb_node->n.len, lb_node->n.s);
	
	/* Place node at the end of string list. */
	prom_lb_node_t **l = &m_lb->lb;
	while (*l != NULL) {
		l = &((*l)->next);
	}
	*l = lb_node;
	lb_node->next = NULL;

	m_lb->n_elem++;
	
	/* Everything went fine. */
	return 0;

error:
	prom_lb_node_free(lb_node, shared_mem);
	return -1;
}

/**
 * Create a list of str (for labels)
 *
 * /param shared_mem 0 means pkg memory otherwise shared one.
 *
 * /return pointer to prom_lb_t struct on success.
 * /return NULL on error.
 */
static prom_lb_t* prom_lb_create(str *lb_str, int shared_mem)
{
	prom_lb_t *m_lb = NULL;

	if (!lb_str || lb_str->len == 0 || lb_str->s == NULL) {
		LM_ERR("No label string\n");
		goto error;
	}
	
	if (shared_mem) {
		/* Shared memory */
		m_lb = (prom_lb_t*)shm_malloc(sizeof(*m_lb));
		if (m_lb == NULL) {
			LM_ERR("shm out of memory\n");
			goto error;
		}
		memset(m_lb, 0, sizeof(*m_lb));

	} else {
		/* Pkg memory */
		m_lb = (prom_lb_t*)pkg_malloc(sizeof(*m_lb));
		if (m_lb == NULL) {
			LM_ERR("pkg out of memory\n");
			goto error;
		}
		memset(m_lb, 0, sizeof(*m_lb));

	} /* if shared_mem */

	/* Add strings to m_lb */
	int len = lb_str->len;
	char *s = lb_str->s;
	int pos_end = 0, pos_start = 0;
	while (pos_end < len) {
		if (s[pos_end] == LABEL_SEP) {
			if (prom_lb_node_add(m_lb, s + pos_start, pos_end - pos_start, shared_mem)) {
				LM_ERR("Cannot add label string\n");
				goto error;
			}
			pos_start = pos_end + 1;
		}

		pos_end++;
	}
	/* Add last string if it does exist. */
	if (pos_end > pos_start) {
		if (prom_lb_node_add(m_lb, s + pos_start, pos_end - pos_start, shared_mem)) {
			LM_ERR("Cannot add label string\n");
			goto error;
		}
	}
	
	/* Everything fine */
	return m_lb;

error:
	prom_lb_free(m_lb, shared_mem);
	return NULL;
}

/**
 * Create a label and add it to a metric.
 *
 * /return 0 on success.
 */
static int prom_label_create(prom_metric_t *mt, str *lb_str)
{
	if (mt == NULL) {
		LM_ERR("No metric available\n");
		return -1;
	}
	
	if (lb_str == NULL || lb_str->len == 0 || lb_str->s == NULL) {
		LM_ERR("No label available\n");
		return -1;
	}

	if (mt->lb_name != NULL) {
		LM_ERR("Label already created\n");
		return -1;
	}

	/* Create new label name in shared memory */
	prom_lb_t *new_lb;
	new_lb = prom_lb_create(lb_str, 1);
	if (new_lb == NULL) {
		LM_ERR("Cannot create label: %.*s\n", lb_str->len, lb_str->s);
		return -1;
	}

	/* Add label name to metric. */
	mt->lb_name = new_lb;

	/* Everything went fine. */
	return 0;
}

/**
 * Create a counter and add it to list.
 */
int prom_counter_create(char *spec)
{
	param_t *pit=NULL;
	param_hooks_t phooks;
	prom_metric_t *m_cnt = NULL;
	str s;

	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &pit)<0)
	{
		LM_ERR("failed parsing params value\n");
		goto error;
	}
	m_cnt = (prom_metric_t*)shm_malloc(sizeof(prom_metric_t));
	if (m_cnt == NULL) {
		LM_ERR("shm out of memory\n");
		goto error;
	}
	memset(m_cnt, 0, sizeof(*m_cnt));
	m_cnt->type = M_COUNTER;
	
	param_t *p = NULL;
	for (p = pit; p; p = p->next) {
		if (p->name.len == 5 && strncmp(p->name.s, "label", 5) == 0) {
			/* Fill counter label. */
			if (prom_label_create(m_cnt, &p->body)) {
				LM_ERR("Error creating label: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("label = %.*s\n", p->body.len, p->body.s);

		} else if (p->name.len == 4 && strncmp(p->name.s, "name", 4) == 0) {
			/* Fill counter name. */
			if (shm_str_dup(&m_cnt->name, &p->body)) {
				LM_ERR("Error creating counter name: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("name = %.*s\n", m_cnt->name.len, m_cnt->name.s);

		} else {
			LM_ERR("Unknown field: %.*s (%.*s)\n", p->name.len, p->name.s,
				   p->body.len, p->body.s);
			goto error;
		}
	} /* for p = pit */

	if (m_cnt->name.s == NULL || m_cnt->name.len == 0) {
		LM_ERR("No counter name\n");
		goto error;
	}

	/* Place counter at the end of list. */
	prom_metric_t **l = &prom_metric_list;
	while (*l != NULL) {
		l = &((*l)->next);
	}
	*l = m_cnt;
	m_cnt->next = NULL;

	/* Everything went fine. */
	return 0;

error:
	if (pit != NULL) {
		free_params(pit);
	}
	if (m_cnt != NULL) {
		prom_counter_free(m_cnt);
	}
	return -1;
}

/**
 * Free a gauge.
 */
static void prom_gauge_free(prom_metric_t *m_gg)
{
	assert(m_gg);

	assert(m_gg->type == M_GAUGE);

	if (m_gg->name.s) {
		shm_free(m_gg->name.s);
	}

	prom_lb_free(m_gg->lb_name, 1);

	prom_lvalue_list_free(m_gg->lval_list);
	
	shm_free(m_gg);
}

/**
 * Create a gauge and add it to list.
 */
int prom_gauge_create(char *spec)
{
	param_t *pit=NULL;
	param_hooks_t phooks;
	prom_metric_t *m_gg = NULL;
	str s;

	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &pit)<0)
	{
		LM_ERR("failed parsing params value\n");
		goto error;
	}
	m_gg = (prom_metric_t*)shm_malloc(sizeof(prom_metric_t));
	if (m_gg == NULL) {
		LM_ERR("shm out of memory\n");
		goto error;
	}
	memset(m_gg, 0, sizeof(*m_gg));
	m_gg->type = M_GAUGE;
	
	param_t *p = NULL;
	for (p = pit; p; p = p->next) {
		if (p->name.len == 5 && strncmp(p->name.s, "label", 5) == 0) {
			/* Fill gauge label. */
			if (prom_label_create(m_gg, &p->body)) {
				LM_ERR("Error creating label: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("label = %.*s\n", p->body.len, p->body.s);

		} else if (p->name.len == 4 && strncmp(p->name.s, "name", 4) == 0) {
			/* Fill gauge name. */
			if (shm_str_dup(&m_gg->name, &p->body)) {
				LM_ERR("Error creating gauge name: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("name = %.*s\n", m_gg->name.len, m_gg->name.s);

		} else {
			LM_ERR("Unknown field: %.*s (%.*s)\n", p->name.len, p->name.s,
				   p->body.len, p->body.s);
			goto error;
		}
	} /* for p = pit */

	if (m_gg->name.s == NULL || m_gg->name.len == 0) {
		LM_ERR("No gauge name\n");
		goto error;
	}

	/* Place gauge at the end of list. */
	prom_metric_t **l = &prom_metric_list;
	while (*l != NULL) {
		l = &((*l)->next);
	}
	*l = m_gg;
	m_gg->next = NULL;

	/* Everything went fine. */
	return 0;

error:
	if (pit != NULL) {
		free_params(pit);
	}
	if (m_gg != NULL) {
		prom_gauge_free(m_gg);
	}
	return -1;
}

/**
 * Add some positive amount to a counter.
 */
int prom_counter_inc(str *s_name, int number, str *l1, str *l2, str *l3)
{
	lock_get(prom_lock);

	/* Find a lvalue based on its metric name and labels. */
	prom_lvalue_t *p = NULL;
	p = prom_metric_lvalue_get(s_name, M_COUNTER, l1, l2, l3);
	if (!p) {
		LM_ERR("Cannot find counter: %.*s\n", s_name->len, s_name->s);
		lock_release(prom_lock);
		return -1;
	}

	/* Add to counter value. */
	p->m.cval += number;
	
	lock_release(prom_lock);
	return 0;
}

/**
 * Reset a counter.
 */
int prom_counter_reset(str *s_name, str *l1, str *l2, str *l3)
{
	lock_get(prom_lock);

	/* Find a lvalue based on its metric name and labels. */
	prom_lvalue_t *p = NULL;
	p = prom_metric_lvalue_get(s_name, M_COUNTER, l1, l2, l3);
	if (!p) {
		LM_ERR("Cannot find counter: %.*s\n", s_name->len, s_name->s);
		lock_release(prom_lock);
		return -1;
	}

	/* Reset counter value. */
	p->m.cval = 0;
	
	lock_release(prom_lock);
	return 0;
}

/**
 * Set a value in a gauge.
 */
int prom_gauge_set(str *s_name, double number, str *l1, str *l2, str *l3)
{
	lock_get(prom_lock);

	/* Find a lvalue based on its metric name and labels. */
	prom_lvalue_t *p = NULL;
	p = prom_metric_lvalue_get(s_name, M_GAUGE, l1, l2, l3);
	if (!p) {
		LM_ERR("Cannot find gauge: %.*s\n", s_name->len, s_name->s);
		lock_release(prom_lock);
		return -1;
	}

	/* Set gauge value. */
	p->m.gval = number;
		
	lock_release(prom_lock);
	return 0;
}

/**
 * Reset value in a gauge.
 */
int prom_gauge_reset(str *s_name, str *l1, str *l2, str *l3)
{
	lock_get(prom_lock);

	/* Find a lvalue based on its metric name and labels. */
	prom_lvalue_t *p = NULL;
	p = prom_metric_lvalue_get(s_name, M_GAUGE, l1, l2, l3);
	if (!p) {
		LM_ERR("Cannot find gauge: %.*s\n", s_name->len, s_name->s);
		lock_release(prom_lock);
		return -1;
	}

	/* Reset counter value. */
	p->m.gval = 0.0;
		
	lock_release(prom_lock);
	return 0;
}

/**
 * Print labels.
 *
 * /return 0 on success.
 */
static int prom_label_print(prom_ctx_t *ctx, prom_lb_t *lb_name, prom_lb_t *plval)
{
	if (!ctx) {
		LM_ERR("No context\n");
		goto error;
	}

	if (!plval) {
		goto error;
	}

	if (plval->n_elem == 0) {
		/* Nothing to print. */
		return 0;
	}	
	
	if (!lb_name || lb_name->n_elem == 0) {
		/* Nothing to print. */
		return 0;
	}

	prom_lb_node_t *lb_name_node = lb_name->lb;
	prom_lb_node_t *plval_node = plval->lb;
	while (lb_name_node && plval_node) {
		if (lb_name_node == lb_name->lb) {
			/* First label */
			if (prom_body_printf(ctx,
								 "{"
					) == -1) {
				LM_ERR("Fail to print\n");
				goto error;
			}
		} else {
			/* Not the first label */
			if (prom_body_printf(ctx,
								 ", "
					) == -1) {
				LM_ERR("Fail to print\n");
				goto error;
			}
		}
		
		if (prom_body_printf(ctx,
							 "%.*s=\"%.*s\"",
							 lb_name_node->n.len, lb_name_node->n.s,
							 plval_node->n.len, plval_node->n.s
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}

		lb_name_node = lb_name_node->next;
		plval_node = plval_node->next;
	} /* while (lb_name_node && plval_node) */

	/* Close labels. */
	if (prom_body_printf(ctx,
						 "}"
			) == -1) {
		LM_ERR("Fail to print\n");
		goto error;
	}

	/* Everything fine. */
	return 0;
	
error:

	return -1;
}

/**
 * Print a user defined metric lvalue pair.
 *
 * /return 0 on success.
 */
static int prom_metric_lvalue_print(prom_ctx_t *ctx, prom_metric_t *p, prom_lvalue_t *pvl)
{
	if (!ctx) {
		LM_ERR("No context\n");
		goto error;
	}

	if (!p) {
		LM_ERR("No metric\n");
		goto error;
	}

	if (!pvl) {
		LM_ERR("No lvalue structure\n");
		goto error;
	}
	
	if (p->type == M_COUNTER) {

		uint64_t ts;
		if (get_timestamp(&ts)) {
			LM_ERR("Fail to get timestamp\n");
			goto error;
		}
		LM_DBG("Counter kamailio_%.*s %" PRIu64 "\n",
			   p->name.len, p->name.s,
			   ts
			);
		if (prom_body_printf(ctx,
							 "kamailio_%.*s",
							 p->name.len, p->name.s
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}

		/* Print labels */
		if (prom_label_print(ctx, p->lb_name, &pvl->lval)) {
			LM_ERR("Fail to print labels\n");
			goto error;
		}

		if (prom_body_printf(ctx,
							 " %" PRIu64,
							 pvl->m.cval
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}

		if (prom_body_printf(ctx,
							 " %" PRIu64 "\n",
							 ts
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}

	} else if (p->type == M_GAUGE) {
		uint64_t ts;
		if (get_timestamp(&ts)) {
			LM_ERR("Fail to get timestamp\n");
			goto error;
		}
		LM_DBG("Gauge kamailio_%.*s %" PRId64 "\n",
			   p->name.len, p->name.s,
			   ts
			);
		
		if (prom_body_printf(ctx,
							 "kamailio_%.*s",
							 p->name.len, p->name.s
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}
		
		/* Print labels */
		if (prom_label_print(ctx, p->lb_name, &pvl->lval)) {
			LM_ERR("Fail to print labels\n");
			goto error;
		}

		if (prom_body_printf(ctx,
							 " %f",
							 pvl->m.gval
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}

		if (prom_body_printf(ctx,
							 " %" PRIu64 "\n",
							 ts
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}
		
	} else {
		LM_DBG("Unknown metric type: %d\n", p->type);
	}

	/* Everything went fine. */
	return 0;

error:
	return -1;
}

/**
 * Print user defined metrics.
 *
 * /return 0 on success.
 */
int prom_metric_list_print(prom_ctx_t *ctx)
{
	lock_get(prom_lock);
	
	prom_metric_t *p = prom_metric_list;
	if (p) {
		if (prom_body_printf(
				ctx, "# User defined metrics\n") == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}
	} else {
		if (prom_body_printf(
				ctx, "# NO User defined metrics\n") == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}
	} /* if p */

	while (p) {

		prom_lvalue_t *pvl = p->lval_list;
		
		while (pvl) {
			if (prom_metric_lvalue_print(ctx, p, pvl)) {
				LM_ERR("Failed to print\n");
				goto error;
			}
			
			pvl = pvl->next;
			
		} /* while pvl */

		p = p->next;
		
	} /* while p */

	lock_release(prom_lock);
	return 0;

error:
	lock_release(prom_lock);
	return -1;
}

