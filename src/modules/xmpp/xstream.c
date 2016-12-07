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

/* xode_stream is a way to have a consistent method of handling incoming XML Stream based events... it doesn't handle the generation of an XML Stream, but provides some facilities to help do that */


static void _xode_put_expatattribs(xode owner, const char** atts)
{
    int i = 0;
    if (atts == NULL) return;
    while (atts[i] != '\0')
    {
        xode_put_attrib(owner, atts[i], atts[i+1]);
        i += 2;
    }
}

/******* internal expat callbacks *********/
static void _xode_stream_startElement(xode_stream xs, const char* name, const char** atts)
{
    xode_pool p;

    /* if xode_stream is bad, get outa here */
    if(xs->status > XODE_STREAM_NODE) return;

    if(xs->node == NULL)
    {
        p = xode_pool_heap(5*1024); /* 5k, typically 1-2k each plus copy of self and workspace */
        xs->node = xode_new_frompool(p,name);
        _xode_put_expatattribs(xs->node, atts);

        if(xs->status == XODE_STREAM_ROOT)
        {
            xs->status = XODE_STREAM_NODE; /* flag status that we're processing nodes now */
            (xs->f)(XODE_STREAM_ROOT, xs->node, xs->arg); /* send the root, f must free all nodes */
            xs->node = NULL;
        }
    }else{
        xs->node = xode_insert_tag(xs->node, name);
        _xode_put_expatattribs(xs->node, atts);
    }

    /* depth check */
    xs->depth++;
    if(xs->depth > XODE_STREAM_MAXDEPTH)
        xs->status = XODE_STREAM_ERROR;
}


static void _xode_stream_endElement(xode_stream xs, const char* name)
{
    xode parent;

    /* if xode_stream is bad, get outa here */
    if(xs->status > XODE_STREAM_NODE) return;

    /* if it's already NULL we've received </stream>, tell the app and we're outta here */
    if(xs->node == NULL)
    {
        xs->status = XODE_STREAM_CLOSE;
        (xs->f)(XODE_STREAM_CLOSE, NULL, xs->arg);
    }else{
        parent = xode_get_parent(xs->node);

        /* we are the top-most node, feed to the app who is responsible to delete it */
        if(parent == NULL)
            (xs->f)(XODE_STREAM_NODE, xs->node, xs->arg);

        xs->node = parent;
    }
    xs->depth--;
}


static void _xode_stream_charData(xode_stream xs, const char *str, int len)
{
    /* if xode_stream is bad, get outa here */
    if(xs->status > XODE_STREAM_NODE) return;

    if(xs->node == NULL)
    {
        /* we must be in the root of the stream where CDATA is irrelevant */
        return;
    }

    xode_insert_cdata(xs->node, str, len);
}


static void _xode_stream_cleanup(void *arg)
{
    xode_stream xs = (xode_stream)arg;

    xode_free(xs->node); /* cleanup anything left over */
    XML_ParserFree(xs->parser);
}


/* creates a new xode_stream with given pool, xode_stream will be cleaned up w/ pool */
xode_stream xode_stream_new(xode_pool p, xode_stream_onNode f, void *arg)
{
    xode_stream newx;

    if(p == NULL || f == NULL)
    {
        fprintf(stderr,"Fatal Programming Error: xode_streamnew() was improperly called with NULL.\n");
        return NULL;
    }

    newx = xode_pool_malloco(p, sizeof(_xode_stream));
    newx->p = p;
    newx->f = f;
    newx->arg = arg;

    /* create expat parser and ensure cleanup */
    newx->parser = XML_ParserCreate(NULL);
    XML_SetUserData(newx->parser, (void *)newx);
    XML_SetElementHandler(newx->parser,
		(void (*)(void*, const char*, const char**))_xode_stream_startElement,
		(void (*)(void*, const char*))_xode_stream_endElement);
    XML_SetCharacterDataHandler(newx->parser, 
		(void (*)(void*, const char*, int))_xode_stream_charData);
    xode_pool_cleanup(p, _xode_stream_cleanup, (void *)newx);

    return newx;
}

/* attempts to parse the buff onto this stream firing events to the handler, returns the last known status */
int xode_stream_eat(xode_stream xs, char *buff, int len)
{
    char *err;
    xode xerr;
    static char maxerr[] = "maximum node size reached";
    static char deeperr[] = "maximum node depth reached";

    if(xs == NULL)
    {
        fprintf(stderr,"Fatal Programming Error: xode_streameat() was improperly called with NULL.\n");
        return XODE_STREAM_ERROR;
    }

    if(len == 0 || buff == NULL)
        return xs->status;

    if(len == -1) /* easy for hand-fed eat calls */
        len = strlen(buff);

    if(!XML_Parse(xs->parser, buff, len, 0))
    {
        err = (char *)XML_ErrorString(XML_GetErrorCode(xs->parser));
        xs->status = XODE_STREAM_ERROR;
    }else if(xode_pool_size(xode_get_pool(xs->node)) > XODE_STREAM_MAXNODE || xs->cdata_len > XODE_STREAM_MAXNODE){
        err = maxerr;
        xs->status = XODE_STREAM_ERROR;
    }else if(xs->status == XODE_STREAM_ERROR){ /* set within expat handlers */
        err = deeperr;
    }else{
        err = deeperr;
    }

    /* fire parsing error event, make a node containing the error string */
    if(xs->status == XODE_STREAM_ERROR)
    {
        xerr = xode_new("error");
        xode_insert_cdata(xerr,err,-1);
        (xs->f)(XODE_STREAM_ERROR, xerr, xs->arg);
    }

    return xs->status;
}
