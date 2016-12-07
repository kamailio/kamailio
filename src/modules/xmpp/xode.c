/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  Jabber
 *  Copyright (C) 1998-1999 The Jabber Team http://jabber.org/
 */
/*! \file
 * \ingroup xmpp
 */


#include "xode.h"

static int _xode_strcmp(const char *a, const char *b)
{
    if(a == NULL || b == NULL) return -1;

    return strcmp(a,b);
}

/*! \brief Internal routines */
static xode _xode_new(xode_pool p, const char* name, unsigned int type)
{
    xode result = NULL;
    if (type > XODE_TYPE_LAST)
        return NULL;

    if (type != XODE_TYPE_CDATA && name == NULL)
        return NULL;

    if (p == NULL)
    {
        p = xode_pool_heap(1*1024);
    }

    /* Allocate & zero memory */
    result = (xode)xode_pool_malloc(p, sizeof(_xode));
    memset(result, '\0', sizeof(_xode));

    /* Initialize fields */
    if (type != XODE_TYPE_CDATA)
        result->name = xode_pool_strdup(p,name);
    result->type = type;
    result->p = p;
    return result;
}

static xode _xode_appendsibling(xode lastsibling, const char* name, unsigned int type)
{
    xode result;

    result = _xode_new(xode_get_pool(lastsibling), name, type);
    if (result != NULL)
    {
        /* Setup sibling pointers */
        result->prev = lastsibling;
        lastsibling->next = result;
    }
    return result;
}

static xode _xode_insert(xode parent, const char* name, unsigned int type)
{
    xode result;

    if(parent == NULL || name == NULL) return NULL;

    /* If parent->firstchild is NULL, simply create a new node for the first child */
    if (parent->firstchild == NULL)
    {
        result = _xode_new(parent->p, name, type);
        parent->firstchild = result;
    }
    /* Otherwise, append this to the lastchild */
    else
    {
        result= _xode_appendsibling(parent->lastchild, name, type);
    }
    result->parent = parent;
    parent->lastchild = result;
    return result;

}

static xode _xode_search(xode firstsibling, const char* name, unsigned int type)
{
    xode current;

    /* Walk the sibling list, looking for a XODE_TYPE_TAG xode with
    the specified name */
    current = firstsibling;
    while (current != NULL)
    {
        if (name != NULL && (current->type == type) && (_xode_strcmp(current->name, name) == 0))
            return current;
        else
            current = current->next;
    }
    return NULL;
}

static char* _xode_merge(xode_pool p, char* dest, unsigned int destsize, const char* src, unsigned int srcsize)
{
    char* result;
    result = (char*)xode_pool_malloc(p, destsize + srcsize + 1);
    memcpy(result, dest, destsize);
    memcpy(result+destsize, src, srcsize);
    result[destsize + srcsize] = '\0';

    /* WARNING: major ugly hack: since we're throwing the old data away, let's jump in the xode_pool and subtract it from the size, this is for xmlstream's big-node checking */
    p->size -= destsize;

    return result;
}

static void _xode_hidesibling(xode child)
{
    if(child == NULL)
        return;

    if(child->prev != NULL)
        child->prev->next = child->next;
    if(child->next != NULL)
        child->next->prev = child->prev;
}

static void _xode_tag2str(xode_spool s, xode node, int flag)
{
    xode tmp;

    if(flag==0 || flag==1)
    {
	    xode_spooler(s,"<",xode_get_name(node),s);
	    tmp = xode_get_firstattrib(node);
	    while(tmp) {
	        xode_spooler(s," ",xode_get_name(tmp),"='",xode_strescape(xode_get_pool(node),xode_get_data(tmp)),"'",s);
	        tmp = xode_get_nextsibling(tmp);
	    }
	    if(flag==0)
	        xode_spool_add(s,"/>");
	    else
	        xode_spool_add(s,">");
    }
    else
    {
	    xode_spooler(s,"</",xode_get_name(node),">",s);
    }
}

static xode_spool _xode_tospool(xode node)
{
    xode_spool s;
    int level=0,dir=0;
    xode tmp;

    if(!node || xode_get_type(node) != XODE_TYPE_TAG)
	return NULL;

    s = xode_spool_newfrompool(xode_get_pool(node));
    if(!s) return(NULL);

    while(1)
    {
        if(dir==0)
        {
    	    if(xode_get_type(node) == XODE_TYPE_TAG)
            {
		        if(xode_has_children(node))
                {
		            _xode_tag2str(s,node,1);
        		    node = xode_get_firstchild(node);
		            level++;
		            continue;
        		}
                else
                {
		            _xode_tag2str(s,node,0);
		        }
	        }
            else
            {
		        xode_spool_add(s,xode_strescape(xode_get_pool(node),xode_get_data(node)));
	        }
	    }

    	tmp = xode_get_nextsibling(node);
	    if(!tmp)
        {
	        node = xode_get_parent(node);
	        level--;
	        if(level>=0) _xode_tag2str(s,node,2);
	        if(level<1) break;
	        dir = 1;
	    }
        else
        {
	        node = tmp;
	        dir = 0;
	    }
    }

    return s;
}


/* External routines */


/*
 *  xode_new_tag -- create a tag node
 *  Automatically creates a memory xode_pool for the node.
 *
 *  parameters
 *      name -- name of the tag
 *
 *  returns
 *      a pointer to the tag node
 *      or NULL if it was unsuccessful
 */
xode xode_new(const char* name)
{
    return _xode_new(NULL, name, XODE_TYPE_TAG);
}

/*
 * alias for 'xode_new'
 */
xode xode_new_tag(const char* name)
{
    return _xode_new(NULL, name, XODE_TYPE_TAG);
}

/*
 *  xode_new_tag_pool -- create a tag node within given pool
 *
 *  parameters
 *      p -- previously created memory pool
 *      name -- name of the tag
 *
 *  returns
 *      a pointer to the tag node
 *      or NULL if it was unsuccessful
 */
xode xode_new_frompool(xode_pool p, const char* name)
{
    return _xode_new(p, name, XODE_TYPE_TAG);
}


/*
 *  xode_insert_tag -- append a child tag to a tag
 *
 *  parameters
 *      parent -- pointer to the parent tag
 *      name -- name of the child tag
 *
 *  returns
 *      a pointer to the child tag node
 *      or NULL if it was unsuccessful
 */
xode xode_insert_tag(xode parent, const char* name)
{
    return _xode_insert(parent, name, XODE_TYPE_TAG);
}


/*
 *  xode_insert_cdata -- append character data to a tag
 *  If last child of the parent is CDATA, merges CDATA nodes. Otherwise
 *  creates a CDATA node, and appends it to the parent's child list.
 *
 *  parameters
 *      parent -- parent tag
 *      CDATA -- character data
 *      size -- size of CDATA
 *              or -1 for null-terminated CDATA strings
 *
 *  returns
 *      a pointer to the child CDATA node
 *      or NULL if it was unsuccessful
 */
xode xode_insert_cdata(xode parent, const char* CDATA, unsigned int size)
{
    xode result;

    if(CDATA == NULL || parent == NULL)
        return NULL;

    if(size == -1)
        size = strlen(CDATA);

    if ((parent->lastchild != NULL) && (parent->lastchild->type == XODE_TYPE_CDATA))
    {
        result = parent->lastchild;
        result->data = _xode_merge(result->p, result->data, result->data_sz, CDATA, size);
        result->data_sz = result->data_sz + size;
    }
    else
    {
        result = _xode_insert(parent, "", XODE_TYPE_CDATA);
        if (result != NULL)
        {
            result->data = (char*)xode_pool_malloc(result->p, size + 1);
            memcpy(result->data, CDATA, size);
            result->data[size] = '\0';
            result->data_sz = size;
        }
    }

    return result;
}


/*
 *  xode_gettag -- find given tag in an xode tree
 *
 *  parameters
 *      parent -- pointer to the parent tag
 *      name -- "name" for the child tag of that name
 *              "name/name" for a sub child (recurses)
 *              "?attrib" to match the first tag with that attrib defined
 *              "?attrib=value" to match the first tag with that attrib and value
 *              or any combination: "name/name/?attrib", etc
 *
 *  results
 *      a pointer to the tag matching search criteria
 *      or NULL if search was unsuccessful
 */
xode xode_get_tag(xode parent, const char* name)
{
    char *str, *slash, *qmark, *equals;
    xode step, ret;

    if(parent == NULL || parent->firstchild == NULL || name == NULL || name == '\0') return NULL;

    if(strstr(name, "/") == NULL && strstr(name,"?") == NULL)
        return _xode_search(parent->firstchild, name, XODE_TYPE_TAG);

    /* jer's note: why can't I modify the name directly, why do I have to strdup it?  damn c grrr! */
    str = strdup(name);
    slash = strstr(str, "/");
    qmark = strstr(str, "?");
    equals = strstr(str, "=");

    if(qmark != NULL && (slash == NULL || qmark < slash))
    { /* of type ?attrib */

        *qmark = '\0';
        qmark++;
        if(equals != NULL)
        {
            *equals = '\0';
            equals++;
        }

        for(step = parent->firstchild; step != NULL; step = xode_get_nextsibling(step))
        {
            if(xode_get_type(step) != XODE_TYPE_TAG)
                continue;

            if(*str != '\0')
                if(_xode_strcmp(xode_get_name(step),str) != 0)
                    continue;

            if(xode_get_attrib(step,qmark) == NULL)
                continue;

            if(equals != NULL && _xode_strcmp(xode_get_attrib(step,qmark),equals) != 0)
                continue;

            break;
        }

        free(str);
        return step;
    }


    *slash = '\0';
    ++slash;

    for(step = parent->firstchild; step != NULL; step = xode_get_nextsibling(step))
    {
        if(xode_get_type(step) != XODE_TYPE_TAG) continue;

        if(_xode_strcmp(xode_get_name(step),str) != 0)
            continue;

        ret = xode_get_tag(step, slash);
        if(ret != NULL)
        {
            free(str);
            return ret;
        }
    }

    free(str);
    return NULL;
}


/* return the cdata from any tag */
char *xode_get_tagdata(xode parent, const char *name)
{
    xode tag;

    tag = xode_get_tag(parent, name);
    if(tag == NULL) return NULL;

    return xode_get_data(tag);
}


void xode_put_attrib(xode owner, const char* name, const char* value)
{
    xode attrib;

    if(owner == NULL || name == NULL || value == NULL) return;

    /* If there are no existing attributes, allocate a new one to start
    the list */
    if (owner->firstattrib == NULL)
    {
        attrib = _xode_new(owner->p, name, XODE_TYPE_ATTRIB);
        owner->firstattrib = attrib;
        owner->lastattrib  = attrib;
    }
    else
    {
        attrib = _xode_search(owner->firstattrib, name, XODE_TYPE_ATTRIB);
        if(attrib == NULL)
        {
            attrib = _xode_appendsibling(owner->lastattrib, name, XODE_TYPE_ATTRIB);
            owner->lastattrib = attrib;
        }
    }
    /* Update the value of the attribute */
    attrib->data_sz = strlen(value);
    attrib->data    = xode_pool_strdup(owner->p, value);

}

char* xode_get_attrib(xode owner, const char* name)
{
    xode attrib;

    if (owner != NULL && owner->firstattrib != NULL)
    {
        attrib = _xode_search(owner->firstattrib, name, XODE_TYPE_ATTRIB);
        if (attrib != NULL)
            return (char*)attrib->data;
    }
    return NULL;
}

void xode_put_vattrib(xode owner, const char* name, void *value)
{
    xode attrib;

    if (owner != NULL)
    {
        attrib = _xode_search(owner->firstattrib, name, XODE_TYPE_ATTRIB);
        if (attrib == NULL)
        {
            xode_put_attrib(owner, name, "");
            attrib = _xode_search(owner->firstattrib, name, XODE_TYPE_ATTRIB);
        }
        if (attrib != NULL)
            attrib->firstchild = (xode)value;
    }
}

void* xode_get_vattrib(xode owner, const char* name)
{
    xode attrib;

    if (owner != NULL && owner->firstattrib != NULL)
    {
        attrib = _xode_search(owner->firstattrib, name, XODE_TYPE_ATTRIB);
        if (attrib != NULL)
            return (void*)attrib->firstchild;
    }
    return NULL;
}

xode xode_get_firstattrib(xode parent)
{
    if (parent != NULL)
        return parent->firstattrib;
    return NULL;
}

xode xode_get_firstchild(xode parent)
{
    if (parent != NULL)
        return parent->firstchild;
    return NULL;
}

xode xode_get_lastchild(xode parent)
{
    if (parent != NULL)
        return parent->lastchild;
    return NULL;
}

xode xode_get_nextsibling(xode sibling)
{
    if (sibling != NULL)
        return sibling->next;
    return NULL;
}

xode xode_get_prevsibling(xode sibling)
{
    if (sibling != NULL)
        return sibling->prev;
    return NULL;
}

xode xode_get_parent(xode node)
{
    if (node != NULL)
        return node->parent;
    return NULL;
}

char* xode_get_name(xode node)
{
    if (node != NULL)
        return node->name;
    return NULL;
}

char* xode_get_data(xode node)
{
    xode cur;

    if(node == NULL) return NULL;

    if(xode_get_type(node) == XODE_TYPE_TAG) /* loop till we find a CDATA */
    {
        for(cur = xode_get_firstchild(node); cur != NULL; cur = xode_get_nextsibling(cur))
            if(xode_get_type(cur) == XODE_TYPE_CDATA)
                return cur->data;
    }else{
        return node->data;
    }
    return NULL;
}

int xode_get_datasz(xode node)
{
	
    if( node == NULL )
    {
        return (int)(long)NULL;	    
    }	    
    else if(xode_get_type(node) == XODE_TYPE_TAG) /* loop till we find a CDATA */
    {
    	xode cur;	
        for(cur = xode_get_firstchild(node); cur != NULL; cur = xode_get_nextsibling(cur))
            if(xode_get_type(cur) == XODE_TYPE_CDATA)
                return cur->data_sz;
    }else{
        return node->data_sz;
    }
    return (int)(long)NULL;
}

int xode_get_type(xode node)
{
    if (node != NULL)
    {
        return node->type;
    }
    return (int)(long)NULL;
}

int xode_has_children(xode node)
{
    if ((node != NULL) && (node->firstchild != NULL))
        return 1;
    return 0;
}

int xode_has_attribs(xode node)
{
    if ((node != NULL) && (node->firstattrib != NULL))
        return 1;
    return 0;
}

xode_pool xode_get_pool(xode node)
{
    if (node != NULL)
        return node->p;
    return (xode_pool)NULL;
}

void xode_hide(xode child)
{
    xode parent;

    if(child == NULL || child->parent == NULL)
        return;

    parent = child->parent;

    /* first fix up at the child level */
    _xode_hidesibling(child);

    /* next fix up at the parent level */
    if(parent->firstchild == child)
        parent->firstchild = child->next;
    if(parent->lastchild == child)
        parent->lastchild = child->prev;
}

void xode_hide_attrib(xode parent, const char *name)
{
    xode attrib;

    if(parent == NULL || parent->firstattrib == NULL || name == NULL)
        return;

    attrib = _xode_search(parent->firstattrib, name, XODE_TYPE_ATTRIB);
    if(attrib == NULL)
        return;

    /* first fix up at the child level */
    _xode_hidesibling(attrib);

    /* next fix up at the parent level */
    if(parent->firstattrib == attrib)
        parent->firstattrib = attrib->next;
    if(parent->lastattrib == attrib)
        parent->lastattrib = attrib->prev;
}



/*
 *  xode2str -- convert given xode tree into a string
 *
 *  parameters
 *      node -- pointer to the xode structure
 *
 *  results
 *      a pointer to the created string
 *      or NULL if it was unsuccessful
 */
char *xode_to_str(xode node)
{
     return xode_spool_tostr(_xode_tospool(node));
}


/* loop through both a and b comparing everything, attribs, cdata, children, etc */
int xode_cmp(xode a, xode b)
{
    int ret = 0;

    while(1)
    {
        if(a == NULL && b == NULL)
            return 0;

        if(a == NULL || b == NULL)
            return -1;

        if(xode_get_type(a) != xode_get_type(b))
            return -1;

        switch(xode_get_type(a))
        {
        case XODE_TYPE_ATTRIB:
            ret = _xode_strcmp(xode_get_name(a), xode_get_name(b));
            if(ret != 0)
                return -1;
            ret = _xode_strcmp(xode_get_data(a), xode_get_data(b));
            if(ret != 0)
                return -1;
            break;
        case XODE_TYPE_TAG:
            ret = _xode_strcmp(xode_get_name(a), xode_get_name(b));
            if(ret != 0)
                return -1;
            ret = xode_cmp(xode_get_firstattrib(a), xode_get_firstattrib(b));
            if(ret != 0)
                return -1;
            ret = xode_cmp(xode_get_firstchild(a), xode_get_firstchild(b));
            if(ret != 0)
                return -1;
            break;
        case XODE_TYPE_CDATA:
            ret = _xode_strcmp(xode_get_data(a), xode_get_data(b));
            if(ret != 0)
                return -1;
        }
        a = xode_get_nextsibling(a);
        b = xode_get_nextsibling(b);
    }
}


xode xode_insert_tagnode(xode parent, xode node)
{
    xode child;

    child = xode_insert_tag(parent, xode_get_name(node));
    if (xode_has_attribs(node))
        xode_insert_node(child, xode_get_firstattrib(node));
    if (xode_has_children(node))
        xode_insert_node(child, xode_get_firstchild(node));

    return child;
}

/* places copy of node and node's siblings in parent */
void xode_insert_node(xode parent, xode node)
{
    if(node == NULL || parent == NULL)
        return;

    while(node != NULL)
    {
        switch(xode_get_type(node))
        {
        case XODE_TYPE_ATTRIB:
            xode_put_attrib(parent, xode_get_name(node), xode_get_data(node));
            break;
        case XODE_TYPE_TAG:
            xode_insert_tagnode(parent, node);
            break;
        case XODE_TYPE_CDATA:
            xode_insert_cdata(parent, xode_get_data(node), xode_get_datasz(node));
        }
        node = xode_get_nextsibling(node);
    }
}


/* produce full duplicate of x with a new xode_pool, x must be a tag! */
xode xode_dup(xode x)
{
    xode x2;

    if(x == NULL)
        return NULL;

    x2 = xode_new(xode_get_name(x));

    if (xode_has_attribs(x))
        xode_insert_node(x2, xode_get_firstattrib(x));
    if (xode_has_children(x))
        xode_insert_node(x2, xode_get_firstchild(x));

    return x2;
}

xode xode_dup_frompool(xode_pool p, xode x)
{
    xode x2;

    if(x == NULL)
        return NULL;

    x2 = xode_new_frompool(p, xode_get_name(x));

    if (xode_has_attribs(x))
        xode_insert_node(x2, xode_get_firstattrib(x));
    if (xode_has_children(x))
        xode_insert_node(x2, xode_get_firstchild(x));

    return x2;
}

xode xode_wrap(xode x,const char *wrapper)
{
    xode wrap;
    if(x==NULL||wrapper==NULL) return NULL;
    wrap=xode_new_frompool(xode_get_pool(x),wrapper);
    if(wrap==NULL) return NULL;
    wrap->firstchild=x;
    wrap->lastchild=x;
    x->parent=wrap;
    return wrap;
}

void xode_free(xode node)
{
    if(node == NULL)
        return;

    xode_pool_free(node->p);
}


void
_xode_to_prettystr( xode_spool s, xode x, int deep )
{
	int i;
	xode y;

	if(xode_get_type(x) != XODE_TYPE_TAG) return;
	
	for(i=0; i<deep; i++) xode_spool_add(s, "\t");	

	xode_spooler( s , "<" , xode_get_name(x) ,  s );

	y = xode_get_firstattrib(x);
	while( y )
	{
		xode_spooler( s , " " , xode_get_name(y) , "='", xode_get_data(y) , "'" , s );

		y = xode_get_nextsibling( y );
	}
	xode_spool_add(s,">");
	xode_spool_add(s,"\n");
		
	if( xode_get_data(x))
	{
		for(i=0; i<=deep; i++) xode_spool_add(s, "\t");	
		xode_spool_add( s , xode_get_data(x)); 
	}
			
	y = xode_get_firstchild(x);
	while( y )
	{
		_xode_to_prettystr(s , y, deep+1);
		y = xode_get_nextsibling(y);
		xode_spool_add(s,"\n");
	}
		
	for(i=0; i<deep; i++) xode_spool_add(s, "\t");	
	xode_spooler( s , "</" , xode_get_name(x) , ">" , s );

	return;
}

char * 
xode_to_prettystr( xode x )
{
	xode_spool s;

	if( !x) return NULL;
	
	s = xode_spool_newfrompool( xode_get_pool(x));

	_xode_to_prettystr( s , x, 0 );

	return xode_spool_tostr(s);
}

