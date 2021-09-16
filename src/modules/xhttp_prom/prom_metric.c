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
#include <stdlib.h>

#include "../../core/mem/shm_mem.h"
#include "../../core/locking.h"
#include "../../core/ut.h"
#include "../../core/parser/parse_param.h"

#include "xhttp_prom.h"
#include "prom_metric.h"
#include "prom.h"

/**
 * @file
 * @brief xHTTP_PROM :: User defined metrics for Prometheus.
 * @ingroup xhttp_prom
 * - Module: @ref xhttp_prom
 */

/* Every internal function locks and unlocks the metric system. */

/**
 * @brief enumeration of metric types.
 */
typedef enum metric_type {
	M_UNSET = 0,
	M_COUNTER = 1,
	M_GAUGE = 2,
	M_HISTOGRAM = 3
} metric_type_t;

/**
 * @brief Struct to store a string (node of a list)
 */
typedef struct prom_lb_node_s {
	str n;
	struct prom_lb_node_s *next;
} prom_lb_node_t;

/**
 * @brief Struct to store a list of strings (labels)
 */
typedef struct prom_lb_s {
	int n_elem; /**< Number of strings. */
	struct prom_lb_node_s *lb;
} prom_lb_t;

/**
 * @brief Struct to store histogram sum, counters, etc.
 */
typedef struct prom_hist_value_s {
	uint64_t count; /**< Total counter equal Inf */
	double sum; /**< Total sum */
	uint64_t *buckets_count; /**< Array of counters for buckets. */
} prom_hist_value_t;

typedef struct prom_metric_s prom_metric_t;

/**
 * @brief Struct to store a value of a label.
 */
typedef struct prom_lvalue_s {
	prom_lb_t lval; /**< values for labels in current metric. */
	uint64_t ts; /**< timespan. Last time metric was modified. */
	union {
		uint64_t cval; /**< Counter value. */
		double gval; /**< Gauge value. */
		prom_hist_value_t *hval; /**< Pointer to histogram data. */
	} m;
	struct prom_metric_s *metric; /**< Metric associated to current lvalue. */
	struct prom_lvalue_s *next;
} prom_lvalue_t;

/**
 * @brief Struct to store number of buckets and their upper values.
 */
typedef struct prom_buckets_upper_s {
	int count;                  /**< Number of buckets. */
	double *upper_bounds;       /**< Upper bounds for buckets. */
} prom_buckets_upper_t;

/**
 * @brief Struct to store a metric.
 */
struct prom_metric_s {
	metric_type_t type; /**< Metric type. */
	str name; /**< Name of the metric. */
	struct prom_lb_s *lb_name; /**< Names of labels. */
	struct prom_buckets_upper_s *buckets_upper; /**< Upper bounds for buckets. */
	struct prom_lvalue_s *lval_list;
	struct prom_metric_s *next;
};

/**
 * @brief Data related to Prometheus metrics.
 */
static prom_metric_t *prom_metric_list = NULL;
static gen_lock_t *prom_lock = NULL; /**< Lock to protect Prometheus metrics. */
static uint64_t lvalue_timeout; /**< Timeout in milliseconds for old lvalue struct. */

static void prom_counter_free(prom_metric_t *m_cnt);
static void prom_gauge_free(prom_metric_t *m_gg);
static void prom_histogram_free(prom_metric_t *m_hist);
static void prom_metric_free(prom_metric_t *metric);
static void prom_lb_free(prom_lb_t *prom_lb, int shared_mem);
static void prom_lb_node_free(prom_lb_node_t *lb_node, int shared_mem);
static int prom_lb_node_add(prom_lb_t *m_lb, char *s, int len, int shared_mem);
static void prom_lvalue_free(prom_lvalue_t *plv);
static void prom_lvalue_list_free(prom_lvalue_t *plv);
static void prom_histogram_value_free(prom_hist_value_t *phv);

/**
 * @brief Parse a string and convert to double.
 *
 * @param s_number pointer to number string.
 * @param pnumber double passed as reference.
 *
 * @return 0 on success.
 * On error value pointed by pnumber is undefined.
 */
int double_parse_str(str *s_number, double *pnumber)
{
	char *s = NULL;
	
	if (!s_number || !s_number->s || s_number->len == 0) {
		LM_ERR("Bad s_number to convert to double\n");
		goto error;
	}

	if (!pnumber) {
		LM_ERR("No double passed by reference\n");
		goto error;
	}

	/* We generate a zero terminated string. */

	/* We set last character to zero to get a zero terminated string. */
	int len = s_number->len;
	s = pkg_malloc(len + 1);
	if (!s) {
		PKG_MEM_ERROR;
		goto error;
	}
	memcpy(s, s_number->s, len);
	s[len] = '\0'; /* Zero terminated string. */

	/* atof function does not check for errors. */
	double num = atof(s);
	LM_DBG("double number (%.*s) -> %f\n", len, s, num);

	*pnumber = num;
	pkg_free(s);
	return 0;

error:
	if (s) {
		pkg_free(s);
	}
	return -1;
}

/**
 * @brief Free list of Prometheus metrics.
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
 * @brief Initialize user defined metrics.
 */
int prom_metric_init()
{
	/* Initialize timeout. minutes to milliseconds. */
	if (timeout_minutes < 0) {
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
 * @brief Close user defined metrics.
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
 * @brief Free a metric.
 */
static void prom_metric_free(prom_metric_t *metric)
{
	assert(metric);

	if (metric->type == M_COUNTER) {
		prom_counter_free(metric);
	} else if (metric->type == M_GAUGE) {
		prom_gauge_free(metric);
	} else if (metric->type == M_HISTOGRAM) {
		prom_histogram_free(metric);
	} else {
		LM_ERR("Unknown metric: %d\n", metric->type);
		return;
	}
}

/**
 * @brief Free a counter.
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
 * @brief Get a metric based on its name.
 *
 * @return pointer to metric on success.
 * @return NULL on error.
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
 * @brief Compare prom_lb_t structure using some strings.
 *
 * @return 0 if prom_lb_t matches the strings.
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
 * @brief Compare two lval structures.
 *
 * @return 0 if they are the same.
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
 * @brief Free a lvalue structure.
 *
 * Only defined for shared memory.
 */
static void prom_lvalue_free(prom_lvalue_t *plv)
{
	if (plv == NULL) {
		return;
	}

	/* Free hval member of structure. */
	if (plv->metric->type == M_HISTOGRAM && plv->m.hval) {
		prom_histogram_value_free(plv->m.hval);
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
 * @brief Free a list of lvalue structures.
 *
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
 * @brief Fill lvalue data in prom_lvalue_t structure based on three strings.
 *
 * Only defined for shared memory.
 * It does not fill histogram counters and sum.
 *
 * @return 0 on success.
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
 * @brief Create and insert a lvalue structure into a metric.
 *
 * It only works in shared memory.
 * It does not fill internal histogram counters and sum, only name and labels.
 *
 * @return pointer to newly created structure on success.
 * @return NULL on error.
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
		SHM_MEM_ERROR;
		return NULL;
	}
	memset(plv, 0, sizeof(*plv));

	/* Set link to metric */
	plv->metric = p_m;
	
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
 * @brief Find a lvalue based on its labels.
 *
 * If it does not exist it creates a new one and inserts it into the metric.
 *
 * @return pointer to lvalue on success.
 * @return NULL on error.
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
 * @brief Delete old lvalue structures in a metric.
 *
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
 * @brief Delete old lvalue structures in list of metrics.
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
 * @brief Get a lvalue based on its metric name and labels.
 *
 * If metric name exists but no lvalue matches it creates a new lvalue.
 *
 * @return NULL if no lvalue was found or created.
 * @return pointer to lvalue on success.
 */
static prom_lvalue_t* prom_metric_lvalue_get(str *s_name, metric_type_t m_type,
											 str *l1, str *l2, str *l3)
{
	if (!s_name || s_name->len == 0 || s_name->s == NULL) {
		LM_ERR("No name for metric\n");
		return NULL;
	}

	/* Delete old lvalue structures. */
	if (lvalue_timeout > 0) {
		prom_metric_list_timeout_delete();
	}

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
 * @brief Free a node in a list of strings.
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
 * @brief Free a list of str (for labels).
 *
 * @param shared_mem 0 means pkg memory otherwise shared one.
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

#define LABEL_SEP ':'  /**< Field separator for labels. */

/**
 * @brief Add a string to list of strings.
 *
 * @param m_lb pointer to list of strings.
 * @param s whole string.
 * @param pos_start position of first character to add.
 * @param pos_end position after last character to add.
 *
 * @return 0 on success.
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
			SHM_MEM_ERROR;
			goto error;
		}
		memset(lb_node, 0, sizeof(*lb_node));

	} else {
		/* Pkg memory */
		lb_node = (prom_lb_node_t*)pkg_malloc(sizeof(*lb_node));
		if (lb_node == NULL) {
			PKG_MEM_ERROR;
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
			SHM_MEM_ERROR;
			goto error;
		}
		memcpy(lb_node->n.s, s, len);
		lb_node->n.len = len;
		
	} else {
		/* Pkg memory */
		/* We left space for zero at the end. */
		lb_node->n.s = (char*)shm_malloc(len + 1);
		if (lb_node->n.s == NULL) {
			SHM_MEM_ERROR;
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
 * @brief Create a list of str (for labels)
 *
 * @param shared_mem 0 means pkg memory otherwise shared one.
 *
 * @return pointer to prom_lb_t struct on success.
 * @return NULL on error.
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
			SHM_MEM_ERROR;
			goto error;
		}
		memset(m_lb, 0, sizeof(*m_lb));

	} else {
		/* Pkg memory */
		m_lb = (prom_lb_t*)pkg_malloc(sizeof(*m_lb));
		if (m_lb == NULL) {
			PKG_MEM_ERROR;
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
 * @brief Create a label and add it to a metric.
 *
 * @return 0 on success.
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
 * @brief Create a counter and add it to list.
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
		SHM_MEM_ERROR;
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
 * @brief Free a gauge.
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
 * @brief Create a gauge and add it to list.
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
		SHM_MEM_ERROR;
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
 * @brief Updates a counter.
 */
int prom_counter_update(str *s_name, operation operation, int number, str *l1, str *l2, str *l3)
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

	if (operation == DECREMENT && p->m.cval < number) {
		LM_ERR("Counter %.*s cannot have negative value after decrement\n", s_name->len, s_name->s);
		lock_release(prom_lock);
		return -1;
	}

	switch (operation)
	{
		case INCREMENT:
			/* Add to counter value. */
			p->m.cval += number;
			break;
		case DECREMENT:
			/* Decrement from counter value. */
			p->m.cval -= number;
			break;
		default:
			LM_ERR("Unknown counter operation\n");
			lock_release(prom_lock);
			return -1;
	}

	lock_release(prom_lock);
	return 0;
}

/**
 * @brief Reset a counter.
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
 * @brief Set a value in a gauge.
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
 * @brief Reset value in a gauge.
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

#define BUCKET_SEP ':'  /**< Field separator for buckets. */

/**
 * @brief Create upper bounds for histogram buckets.
 *
 * @return 0 on success.
 */
int prom_buckets_create(prom_metric_t *m_hist, str *bucket_str)
{
	assert(m_hist);

	char *s;
	int cnt;
	prom_buckets_upper_t *b_upper = NULL;

	b_upper = (prom_buckets_upper_t*)shm_malloc(sizeof(prom_buckets_upper_t));
	if (b_upper == NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(b_upper, 0, sizeof(*b_upper));

	/* Parse bucket_str */

	if (bucket_str == NULL) {
		LM_DBG("Setting default configuration for histogram buckets\n");

		/* Default bucket configuration. */
		/* [0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10] */

		b_upper->count = 11; /* We do not count Inf bucket. */
		b_upper->upper_bounds = (double*)shm_malloc(sizeof(double) * b_upper->count);
		if (b_upper->upper_bounds == NULL) {
			SHM_MEM_ERROR;
			goto error;
		}
		b_upper->upper_bounds[0] = 0.005;
		b_upper->upper_bounds[1] = 0.01;
		b_upper->upper_bounds[2] = 0.025;
		b_upper->upper_bounds[3] = 0.05;
		b_upper->upper_bounds[4] = 0.1;
		b_upper->upper_bounds[5] = 0.25;
		b_upper->upper_bounds[6] = 0.5;
		b_upper->upper_bounds[7] = 1.0;
		b_upper->upper_bounds[8] = 2.5;
		b_upper->upper_bounds[9] = 5.0;
		b_upper->upper_bounds[10] = 10.0;
		
	} else {
		/* bucket_str exits. */

		if (bucket_str->len == 0 || bucket_str->s == NULL) {
			LM_ERR("Void bucket string\n");
			goto error;
		}

		/* Count number of buckets. */
		cnt = 1; /* At least one bucket. */
		int i = 0;
		s = bucket_str->s;
		while (i < bucket_str->len) {
			if (s[i] == BUCKET_SEP) {
				cnt++;
			}
			i++;
		}
		LM_DBG("Preliminarily found %d buckets\n", cnt);

		b_upper->count = cnt; /* We do not count Inf bucket. */
		b_upper->upper_bounds = (double*)shm_malloc(sizeof(double) * b_upper->count);
		if (b_upper->upper_bounds == NULL) {
			SHM_MEM_ERROR;
			goto error;
		}

		/* Parse bucket_str and fill b_upper->upper_bounds */
		int len = bucket_str->len;
		s = bucket_str->s;
		int pos_end = 0, pos_start = 0;
		cnt = 0;
		while (pos_end < len) {
			if (s[pos_end] == BUCKET_SEP) {
				str st;
				st.len = pos_end - pos_start;
				st.s = s + pos_start;
				if (double_parse_str(&st, b_upper->upper_bounds + cnt)) {
					LM_ERR("Cannot add double to bucket (%d): %.*s\n",
						   cnt, bucket_str->len, bucket_str->s);
					goto error;
				}
				pos_start = pos_end + 1;
				cnt++;
			}

			pos_end++;
		}
		/* Add last string if it does exist. */
		if (pos_end > pos_start) {
			str st;
			st.len = pos_end - pos_start;
			st.s = s + pos_start;
			if (double_parse_str(&st, b_upper->upper_bounds + cnt)) {
				LM_ERR("Cannot add double to bucket (%d): %.*s\n",
					   cnt, bucket_str->len, bucket_str->s);
				goto error;
			}
			cnt++;
		}
		if (cnt < 1) {
			LM_ERR("At least one bucket needed\n");
			goto error;
		}
		if (cnt != b_upper->count) {
			LM_ERR("Wrong number of parsed buckets. Expected %d found %d\n",
				   b_upper->count, cnt);
			goto error;
		}

		/* Check buckets increase. */
		for (cnt = 1; cnt < b_upper->count; cnt++) {
			if (b_upper->upper_bounds[cnt] < b_upper->upper_bounds[cnt - 1]) {
				LM_ERR("Buckets must increase\n");
				goto error;
			}
		}
		
	} /* if (bucket_str == NULL) */
	
	/* Everything OK. */
	m_hist->buckets_upper = b_upper;
	return 0;
	
error:

	if (b_upper) {
		if (b_upper->upper_bounds) {
			shm_free(b_upper->upper_bounds);
		}
		shm_free(b_upper);
	}

	return -1;
}

/**
 * @brief Free a histogram.
 */
static void prom_histogram_free(prom_metric_t *m_hist)
{
	assert(m_hist);

	assert(m_hist->type == M_HISTOGRAM);

	if (m_hist->name.s) {
		shm_free(m_hist->name.s);
	}

	/* Free buckets_upper. */
	if (m_hist->buckets_upper) {
		if (m_hist->buckets_upper->upper_bounds) {
			shm_free(m_hist->buckets_upper->upper_bounds);
		}
		shm_free(m_hist->buckets_upper);
	}
	
	prom_lb_free(m_hist->lb_name, 1);

	prom_lvalue_list_free(m_hist->lval_list);
	
	shm_free(m_hist);
}

/**
 * @brief Create a histogram and add it to list.
 *
 * @return 0 on success.
 */
int prom_histogram_create(char *spec)
{
	param_t *pit=NULL;
	param_hooks_t phooks;
	prom_metric_t *m_hist = NULL;
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
	m_hist = (prom_metric_t*)shm_malloc(sizeof(prom_metric_t));
	if (m_hist == NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(m_hist, 0, sizeof(*m_hist));
	m_hist->type = M_HISTOGRAM;

	param_t *p = NULL;
	for (p = pit; p; p = p->next) {
		if (p->name.len == 5 && strncmp(p->name.s, "label", 5) == 0) {
			/* Fill histogram label. */
			if (prom_label_create(m_hist, &p->body)) {
				LM_ERR("Error creating label: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("label = %.*s\n", p->body.len, p->body.s);

		} else if (p->name.len == 4 && strncmp(p->name.s, "name", 4) == 0) {
			/* Fill histogram name. */
			if (shm_str_dup(&m_hist->name, &p->body)) {
				LM_ERR("Error creating histogram name: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("name = %.*s\n", m_hist->name.len, m_hist->name.s);

		} else if (p->name.len == 7 && strncmp(p->name.s, "buckets", 7) == 0) {
			/* Fill histogram buckets. */
			if (prom_buckets_create(m_hist, &p->body)) {
				LM_ERR("Error creating buckets: %.*s\n", p->body.len, p->body.s);
				goto error;
			}
			LM_DBG("buckets = %.*s\n", p->body.len, p->body.s);
			
		} else {
			LM_ERR("Unknown field: %.*s (%.*s)\n", p->name.len, p->name.s,
				   p->body.len, p->body.s);
			goto error;
		}
	} /* for p = pit */

	if (m_hist->name.s == NULL || m_hist->name.len == 0) {
		LM_ERR("No histogram name\n");
		goto error;
	}

	/* Set default buckets. */
	if (m_hist->buckets_upper == NULL) {
		LM_DBG("Setting default buckets\n");
		if (prom_buckets_create(m_hist, NULL)) {
			LM_ERR("Failed to create default buckets\n");
			goto error;
		}
	}
	
	/* Place histogram at the end of list. */
	prom_metric_t **l = &prom_metric_list;
	while (*l != NULL) {
		l = &((*l)->next);
	}
	*l = m_hist;
	m_hist->next = NULL;

	/* For debugging purpose show upper bounds for buckets. */
	int i;
	for (i = 0; i < m_hist->buckets_upper->count; i++) {
		LM_DBG("Bucket (%d) -> %f\n", i, m_hist->buckets_upper->upper_bounds[i]);
	}
	
	/* Everything went fine. */
	return 0;

error:
	if (pit != NULL) {
		free_params(pit);
	}
	if (m_hist != NULL) {
		prom_histogram_free(m_hist);
	}
	return -1;
}

/**
 * @brief Free a prom_hist_value_t structure.
 */
static void prom_histogram_value_free(prom_hist_value_t *phv)
{
	if (!phv) {
		return;
	}

	if (phv->buckets_count) {
		shm_free(phv->buckets_count);
	}

	shm_free(phv);
}

/**
 * @brief Create prom_hist_value_t structure if needed
 *
 * @return 0 on success.
 */
static int prom_histogram_value_create(prom_lvalue_t *hlv)
{
	assert(hlv);

	if (hlv->m.hval) {
		/* Return if prom_hist_value_t structure already exists. */
		return 0;
	}

	prom_hist_value_t *phv = NULL;
	phv = (prom_hist_value_t*)shm_malloc(sizeof(*phv));
	if (phv == NULL) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(phv, 0, sizeof(*phv));

	int count = hlv->metric->buckets_upper->count;
	LM_DBG("Setting array for %d buckets\n", count);
	phv->buckets_count = (uint64_t*)shm_malloc(sizeof(uint64_t) * count);
	if (!phv->buckets_count) {
		SHM_MEM_ERROR;
		goto error;
	}
	memset(phv->buckets_count, 0, sizeof(uint64_t) * count);
	
	hlv->m.hval = phv;
	return 0;
	
error:
	if (phv) {
		prom_histogram_value_free(phv);
	}

	return -1;
}

/**
 * @brief Observe a value in lvalue for histogram.
 *
 * @return 0 on success.
 */
static int prom_histogram_lvalue_observe(prom_lvalue_t *hlv, double number)
{
	assert(hlv);

	/* Create prom_hist_value_t structure if needed. */
	if (prom_histogram_value_create(hlv)) {
		LM_ERR("Failed to create histogram_value\n");
		goto error;
	}

	int i;
	int cnt = hlv->metric->buckets_upper->count;
	double *buck_up = hlv->metric->buckets_upper->upper_bounds;
	for (i = cnt - 1; i >= 0; i--) {
		if (number <= buck_up[i]) {
			hlv->m.hval->buckets_count[i]++;
		} else {
			break;
		}
	}

	hlv->m.hval->count++;
	hlv->m.hval->sum = hlv->m.hval->sum + number;

	LM_DBG("bucket_count: %" PRIu64 "\n", hlv->m.hval->count);
	LM_DBG("bucket_sum: %f\n", hlv->m.hval->sum);

	for (i = 0; i < cnt; i++) {
		LM_DBG("bucket (%d) [%f] -> %" PRIu64 "\n",
			   i, buck_up[i], hlv->m.hval->buckets_count[i]);
	}
	
	/* Everything fine. */
	return 0;

error:

	return -1;
}

/**
 * @brief Observe a value in a histogram.
 *
 * @param number value to observe.
 */
int prom_histogram_observe(str *s_name, double number, str *l1, str *l2, str *l3)
{
	lock_get(prom_lock);

	/* Find a lvalue based on its metric name and labels. */
	prom_lvalue_t *p = NULL;
	p = prom_metric_lvalue_get(s_name, M_HISTOGRAM, l1, l2, l3);
	if (!p) {
		LM_ERR("Cannot find histogram: %.*s\n", s_name->len, s_name->s);
		goto error;
	}

	/* Observe value in histogram related structure. */
	if (prom_histogram_lvalue_observe(p, number)) {
		LM_ERR("Cannot observe number %f in lvalue for histogram: %.*s\n",
			   number, s_name->len, s_name->s);
		goto error;
	}

	lock_release(prom_lock);
	return 0;

error:
	lock_release(prom_lock);
	return -1;
}

/**
 * @brief Print labels.
 *
 * @return 0 on success.
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
 * @brief Print labels with le label added at the end.
 *
 * @param le_s zero terminated string with le value.
 *
 * @return 0 on success.
 */
static int prom_label_print_le(prom_ctx_t *ctx, prom_lb_t *lb_name, prom_lb_t *plval, char *le_s)
{
	if (!ctx) {
		LM_ERR("No context\n");
		goto error;
	}

	if (prom_body_printf(ctx,
						 "{"
			) == -1) {
		LM_ERR("Fail to print\n");
		goto error;
	}

	if (plval && plval->n_elem != 0 && lb_name && lb_name->n_elem != 0) {
		/* There are labels different from le one. */
		prom_lb_node_t *lb_name_node = lb_name->lb;
		prom_lb_node_t *plval_node = plval->lb;
		while (lb_name_node && plval_node) {
		
			if (prom_body_printf(ctx,
								 "%.*s=\"%.*s\", ",
								 lb_name_node->n.len, lb_name_node->n.s,
								 plval_node->n.len, plval_node->n.s
					) == -1) {
				LM_ERR("Fail to print\n");
				goto error;
			}

			lb_name_node = lb_name_node->next;
			plval_node = plval_node->next;
		} /* while (lb_name_node && plval_node) */

	} /* if plval && plval->n_elem != 0 */

	/* Print le label. */
	if (prom_body_printf(ctx,
						 "le=\"%s\"",
						 le_s
			) == -1) {
		LM_ERR("Fail to print\n");
		goto error;
	}
	
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

#define LE_LABEL_LEN 50 /**< Maximum length of LE label. */

/**
 * @brief Print a user defined metric lvalue pair.
 *
 * @return 0 on success.
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
		LM_DBG("Counter %.*s%.*s %" PRIu64 "\n",
			   xhttp_prom_beginning.len, xhttp_prom_beginning.s,
			   p->name.len, p->name.s,
			   ts
			);
		if (prom_body_printf(ctx,
							 "%.*s%.*s",
							 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
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
		LM_DBG("Gauge %.*s%.*s %" PRId64 "\n",
			   xhttp_prom_beginning.len, xhttp_prom_beginning.s,
			   p->name.len, p->name.s,
			   ts
			);
		
		if (prom_body_printf(ctx,
							 "%.*s%.*s",
							 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
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

	} else if (p->type == M_HISTOGRAM) {
		uint64_t ts;
		if (get_timestamp(&ts)) {
			LM_ERR("Fail to get timestamp\n");
			goto error;
		}
		LM_DBG("Histogram %.*s%.*s %" PRId64 "\n",
			   xhttp_prom_beginning.len, xhttp_prom_beginning.s,
			   p->name.len, p->name.s,
			   ts
			);

		/* Display buckets. */
		char le_val_s[LE_LABEL_LEN];
		int count = p->buckets_upper->count; /* Number of buckets. */
		int i;
		for (i = 0; i < count; i++) {
			/* Display a bucket. */
			if (prom_body_printf(ctx,
								 "%.*s%.*s_bucket",
								 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
								 p->name.len, p->name.s
					) == -1) {
				LM_ERR("Fail to print\n");
				goto error;
			}
		
			/* Print labels */
			snprintf(le_val_s, LE_LABEL_LEN, "%f",
					 pvl->metric->buckets_upper->upper_bounds[i]);
			
			if (prom_label_print_le(ctx, p->lb_name, &pvl->lval, le_val_s)) {
				LM_ERR("Fail to print labels\n");
				goto error;
			}

			if (prom_body_printf(ctx,
								 " %" PRIu64,
								 pvl->m.hval->buckets_count[i]
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

		} /* for i = 0 */
		
		/* Write Inf bucket. */
		if (prom_body_printf(ctx,
							 "%.*s%.*s_bucket",
							 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
							 p->name.len, p->name.s
				) == -1) {
			LM_ERR("Fail to print\n");
			goto error;
		}
		
		/* Print Inf le labels */
		if (prom_label_print_le(ctx, p->lb_name, &pvl->lval, "+Inf")) {
			LM_ERR("Fail to print labels\n");
			goto error;
		}

		if (prom_body_printf(ctx,
							 " %" PRIu64,
							 pvl->m.hval->count
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
		
		/* Display histogram sum. */
		if (prom_body_printf(ctx,
							 "%.*s%.*s_sum",
							 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
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
							 pvl->m.hval->sum
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

		/* Display histogram counter. */
		if (prom_body_printf(ctx,
							 "%.*s%.*s_count",
							 xhttp_prom_beginning.len, xhttp_prom_beginning.s,
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
							 pvl->m.hval->count
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
 * @brief Print user defined metrics.
 *
 * @return 0 on success.
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

