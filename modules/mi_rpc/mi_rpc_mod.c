/*
 * $Id$
 *
 * mi_rpc module
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"

#include "../../lib/kmi/mi.h"
#include "../../rpc.h"

MODULE_VERSION

static str mi_rpc_indent = { "\t", 1 };

static const char* rpc_mi_exec_doc[2] = {
	"Execut MI command",
	0
};

static void rpc_mi_exec(rpc_t* rpc, void* c);

rpc_export_t mr_rpc[] = {
	{"mi",  rpc_mi_exec,  rpc_mi_exec_doc,  RET_ARRAY},
	{0, 0, 0, 0}
};

struct module_exports exports = {
	"mi_rpc",
	0,        /* Exported functions */
	mr_rpc,   /* RPC methods */
	0,        /* Export parameters */
	0,        /* Module initialization function */
	0,        /* Response function */
	0,        /* Destroy function */
	0,        /* OnCancel function */
	0         /* Child initialization function */
};

struct mi_root *mi_rpc_read_params(rpc_t *rpc, void *ctx)
{
	struct mi_root *root;
	struct mi_node *node;
	str name;
	str value;

	root = init_mi_tree(0,0,0);
	if (!root) {
		LM_ERR("the MI tree cannot be initialized!\n");
		goto error;
	}
	node = &root->node;

	while (rpc->scan(ctx, "*S", &value) == 1)
	{
		name.s   = 0;
		name.len = 0;
		
		if(value.len>=2 && value.s[0]=='-' && value.s[1]=='-')
		{
			/* name */
			if(value.len>2)
			{
				name.s   = value.s + 2;
				name.len = value.len - 2;
			}

			/* value */
			if(rpc->scan(ctx, "*S", &value) != 1)
			{
				LM_ERR("value expected\n");
				goto error;
			}
		}

		if(!add_mi_node_child(node, 0, name.s, name.len,
					value.s, value.len))
		{
			LM_ERR("cannot add the child node to the MI tree\n");
			goto error;
		}
	}

	return root;

error:
	if (root)
		free_mi_tree(root);
	return 0;
}

static int mi_rpc_rprint_node(rpc_t* rpc, void* ctx, struct mi_node *node,
		int level)
{
	struct mi_attr *attr;
	char indent[32];
	int i;
	char *p;

	if(level*mi_rpc_indent.len>=32)
	{
		LM_ERR("too many recursive levels for indentation\n");
		return -1;
	}
	p = indent;
	for(i=0; i<level; i++)
	{
		memcpy(p, mi_rpc_indent.s, mi_rpc_indent.len);
		p += mi_rpc_indent.len;
	}
	*p = 0;
	for( ; node ; node=node->next )
	{
		rpc->printf(ctx, "%s+ %.*s:: %.*s",
				indent,
				node->name.len, (node->name.s)?node->name.s:"",
				node->value.len, (node->value.s)?node->value.s:"");
	
		for( attr=node->attributes ; attr!=NULL ; attr=attr->next ) {
			rpc->printf(ctx, "%s%.*s  - %.*s=%.*s",
					indent, mi_rpc_indent.len, mi_rpc_indent.s,
					attr->name.len, (attr->name.s)?attr->name.s:"",
					attr->value.len, (attr->value.s)?attr->value.s:"");
		}

		if (node->kids) {
			if (mi_rpc_rprint_node(rpc, ctx, node->kids, level+1)<0)
				return -1;
		}
	}
	return 0;
}

static int mi_rpc_print_tree(rpc_t* rpc, void* ctx, struct mi_root *tree)
{
	rpc->printf(ctx, "%d %.*s\n", tree->code,
			tree->reason.len, tree->reason.s);

	if (tree->node.kids)
	{
		if (mi_rpc_rprint_node(rpc, ctx, tree->node.kids, 0)<0)
			return -1;
	}
	
	return 0;
}

static void rpc_mi_exec(rpc_t *rpc, void *ctx)
{
	str cmd;
	struct mi_cmd *mic;
	struct mi_root *mi_req;
	struct mi_root *mi_rpl;

	if (rpc->scan(ctx, "S", &cmd) < 1)
	{
		LM_ERR("command parameter not found\n");
		rpc->fault(ctx, 500, "command parameter missing");
		return;
	}

	mic = lookup_mi_cmd(cmd.s, cmd.len);
	if(mic==0)
	{
		LM_ERR("mi command %.*s is not available\n", cmd.len, cmd.s);
		rpc->fault(ctx, 500, "command not available");
		return;
	}

	if (mic->flags&MI_ASYNC_RPL_FLAG)
	{
		LM_ERR("async mi cmd support not implemented yet\n");
		rpc->fault(ctx, 500, "async my cmd not implemented yet");
		return;
	}

	mi_req = 0;
	if(!(mic->flags&MI_NO_INPUT_FLAG))
	{
		mi_req = mi_rpc_read_params(rpc, ctx);
		if(mi_req==NULL)
		{
			LM_ERR("cannot parse parameters\n");
			rpc->fault(ctx, 500, "cannot parse parameters");
			return;
		}
	}
	mi_rpl=run_mi_cmd(mic, mi_req);

	if(mi_rpl == 0)
	{
		rpc->fault(ctx, 500, "execution failed");
		if (mi_req) free_mi_tree(mi_req);
		return;
	}

	if (mi_rpl!=MI_ROOT_ASYNC_RPL)
	{
		mi_rpc_print_tree(rpc, ctx, mi_rpl);
		free_mi_tree(mi_rpl);
		if (mi_req) free_mi_tree(mi_req);
		return;
	}

	/* async cmd -- not yet */
	rpc->fault(ctx, 500, "no async handling yet");
	if (mi_req) free_mi_tree(mi_req);
	return;
}
