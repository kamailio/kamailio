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

xode_pool xode_spool_getpool(const xode_spool s)
{
    if(s == NULL)
        return NULL;

    return s->p;
}

int xode_spool_getlen(const xode_spool s)
{
    if(s == NULL)
        return 0;

    return s->len;    
}

void xode_spool_free(xode_spool s)
{
    xode_pool_free(xode_spool_getpool(s));
}

xode_spool xode_spool_newfrompool(xode_pool p)
{
    xode_spool s;

    s = xode_pool_malloc(p, sizeof(struct xode_spool_struct));
    s->p = p;
    s->len = 0;
    s->last = NULL;
    s->first = NULL;
    return s;
}

xode_spool xode_spool_new(void)
{
    return xode_spool_newfrompool(xode_pool_heap(512));
}

void xode_spool_add(xode_spool s, char *str)
{
    struct xode_spool_node *sn;
    int len;

    if(str == NULL)
        return;

    len = strlen(str);
    if(len == 0)
        return;

    sn = xode_pool_malloc(s->p, sizeof(struct xode_spool_node));
    sn->c = xode_pool_strdup(s->p, str);
    sn->next = NULL;

    s->len += len;
    if(s->last != NULL)
        s->last->next = sn;
    s->last = sn;
    if(s->first == NULL)
        s->first = sn;
}

void xode_spooler(xode_spool s, ...)
{
    va_list ap;
    char *arg = NULL;

    if(s == NULL)
        return;

    va_start(ap, s);

    /* loop till we hit our end flag, the first arg */
    while(1)
    {
        arg = va_arg(ap,char *);
        if((void*)arg == (void*)s || arg == NULL)
            break;
        else
            xode_spool_add(s, arg);
    }

    va_end(ap);
}

char *xode_spool_tostr(xode_spool s)
{
    char *ret,*tmp;
    struct xode_spool_node *next;

    if(s == NULL || s->len == 0 || s->first == NULL)
        return NULL;

    ret = xode_pool_malloc(s->p, s->len + 1);
    *ret = '\0';

    next = s->first;
    tmp = ret;
    while(next != NULL)
    {
        tmp = strcat(tmp,next->c);
        next = next->next;
    }

    return ret;
}

/* convenience :) */
char *xode_spool_str(xode_pool p, ...)
{
    va_list ap;
    xode_spool s;
    char *arg = NULL;

    if(p == NULL)
        return NULL;

    s = xode_spool_newfrompool(p);

    va_start(ap, p);

    /* loop till we hit our end flag, the first arg */
    while(1)
    {
        arg = va_arg(ap,char *);
        if((void*)arg == (void*)p)
            break;
        else
            xode_spool_add(s, arg);
    }

    va_end(ap);

    return xode_spool_tostr(s);
}


char *xode_strunescape(xode_pool p, char *buf)
{
    int i,j=0;
    char *temp;

    if (p == NULL || buf == NULL) return(NULL);

    if (strchr(buf,'&') == NULL) return(buf);

    temp = xode_pool_malloc(p,strlen(buf)+1);

    if (temp == NULL) return(NULL);

    for(i=0;i<strlen(buf);i++)
    {
        if (buf[i]=='&')
        {
            if (strncmp(&buf[i],"&amp;",5)==0)
            {
                temp[j] = '&';
                i += 4;
            } else if (strncmp(&buf[i],"&quot;",6)==0) {
                temp[j] = '\"';
                i += 5;
            } else if (strncmp(&buf[i],"&apos;",6)==0) {
                temp[j] = '\'';
                i += 5;
            } else if (strncmp(&buf[i],"&lt;",4)==0) {
                temp[j] = '<';
                i += 3;
            } else if (strncmp(&buf[i],"&gt;",4)==0) {
                temp[j] = '>';
                i += 3;
            }
        } else {
            temp[j]=buf[i];
        }
        j++;
    }
    temp[j]='\0';
    return(temp);
}


char *xode_strescape(xode_pool p, char *buf)
{
    int i,j,oldlen,newlen;
    char *temp;

    if (p == NULL || buf == NULL) return(NULL);

    oldlen = newlen = strlen(buf);
    for(i=0;i<oldlen;i++)
    {
        switch(buf[i])
        {
        case '&':
            newlen+=5;
            break;
        case '\'':
            newlen+=6;
            break;
        case '\"':
            newlen+=6;
            break;
        case '<':
            newlen+=4;
            break;
        case '>':
            newlen+=4;
            break;
        }
    }

    if(oldlen == newlen) return buf;

    temp = xode_pool_malloc(p,newlen+1);

    if (temp==NULL) return(NULL);

    for(i=j=0;i<oldlen;i++)
    {
        switch(buf[i])
        {
        case '&':
            memcpy(&temp[j],"&amp;",5);
            j += 5;
            break;
        case '\'':
            memcpy(&temp[j],"&apos;",6);
            j += 6;
            break;
        case '\"':
            memcpy(&temp[j],"&quot;",6);
            j += 6;
            break;
        case '<':
            memcpy(&temp[j],"&lt;",4);
            j += 4;
            break;
        case '>':
            memcpy(&temp[j],"&gt;",4);
            j += 4;
            break;
        default:
            temp[j++] = buf[i];
        }
    }
    temp[j] = '\0';
    return temp;
}
