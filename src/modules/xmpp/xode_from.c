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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


static void _xode_put_expatattribs(xode current, const char **atts)
{
    int i = 0;
    if (atts == NULL) return;
    while (atts[i] != '\0')
    {
        xode_put_attrib(current, atts[i], atts[i+1]);
        i += 2;
    }
}

static void _xode_expat_startElement(void* userdata, const char* name, const char** atts)
{
    /* get the xmlnode pointed to by the userdata */
    xode *x = userdata;
    xode current = *x;

    if (current == NULL)
    {
        /* allocate a base node */
        current = xode_new(name);
        _xode_put_expatattribs(current, atts);
        *x = current;
    }
    else
    {
        *x = xode_insert_tag(current, name);
        _xode_put_expatattribs(*x, atts);
    }
}

static void _xode_expat_endElement(void* userdata, const char* name)
{
    xode *x = userdata;
    xode current = *x;

    current->complete = 1;
    current = xode_get_parent(current);

    /* if it's NULL we've hit the top folks, otherwise back up a level */
    if(current != NULL)
        *x = current;
}

static void _xode_expat_charData(void* userdata, const char* s, int len)
{
    xode *x = userdata;
    xode current = *x;

    xode_insert_cdata(current, s, len);
}


xode xode_from_str(char *str, int len)
{
    XML_Parser p;
    xode *x, node; /* pointer to an xmlnode */

    if(NULL == str)
        return NULL;

    if(len == -1)
        len = strlen(str);

    x = malloc(sizeof(void *));

    *x = NULL; /* pointer to NULL */
    p = XML_ParserCreate(NULL);
    XML_SetUserData(p, x);
    XML_SetElementHandler(p, _xode_expat_startElement, _xode_expat_endElement);
    XML_SetCharacterDataHandler(p, _xode_expat_charData);
    if(!XML_Parse(p, str, len, 1))
    {
        /*        jdebug(ZONE,"xmlnode_str_error: %s",(char *)XML_ErrorString(XML_GetErrorCode(p)));*/
        xode_free(*x);
        *x = NULL;
    }
    node = *x;
    free(x);
    XML_ParserFree(p);
    return node; /* return the xmlnode x points to */
}

xode xode_from_strx(char *str, int len, int *err, int *pos)
{
    XML_Parser p;
    xode *x, node; /* pointer to an xmlnode */

    if(NULL == str)
        return NULL;

    if(len == -1)
        len = strlen(str);

    x = malloc(sizeof(void *));

    *x = NULL; /* pointer to NULL */
    p = XML_ParserCreate(NULL);
    XML_SetUserData(p, x);
    XML_SetElementHandler(p, _xode_expat_startElement, _xode_expat_endElement);
    XML_SetCharacterDataHandler(p, _xode_expat_charData);
    XML_Parse(p, str, len, 0);
	if(err != NULL)
		*err = XML_GetErrorCode(p);
	if(pos != NULL)
		*pos = XML_GetCurrentByteIndex(p);		
    node = *x;
    free(x);
    XML_ParserFree(p);
    
	return node; /* return the xmlnode x points to */
}

xode xode_from_file(char *file)
{
    XML_Parser p;
    xode *x, node; /* pointer to an xmlnode */
    char buf[BUFSIZ];
    int done, fd, len;
    char _file[1000];

    if(NULL == file)
        return NULL;

    /* perform tilde expansion */
    if(*file == '~')
    {
        char *env = getenv("HOME");
        if(env != NULL)
            snprintf((char*)_file, 1000, "%s%s", env, file + 1);
        else
            snprintf((char*)_file, 1000, "%s", file);
    }
    else
    {
        snprintf((char*)_file, 1000, "%s", file);
    }

    fd = open((char*)&_file,O_RDONLY);
    if(fd < 0)
        return NULL;

    x = malloc(sizeof(void *));

    *x = NULL; /* pointer to NULL */
    p = XML_ParserCreate(NULL);
    XML_SetUserData(p, x);
    XML_SetElementHandler(p, _xode_expat_startElement, _xode_expat_endElement);
    XML_SetCharacterDataHandler(p, _xode_expat_charData);
    do{
        len = read(fd, buf, BUFSIZ);
        done = len < BUFSIZ;
        if(!XML_Parse(p, buf, len, done))
        {
            /*            jdebug(ZONE,"xmlnode_file_parseerror: %s",(char *)XML_ErrorString(XML_GetErrorCode(p)));*/
            xode_free(*x);
            *x = NULL;
            done = 1;
        }
    }while(!done);

    node = *x;
    XML_ParserFree(p);
    free(x);
    close(fd);
    return node; /* return the xmlnode x points to */
}

int xode_to_file(char *file, xode node)
{
    char *doc;
    int fd, i;
    char _file[1000];

    if(file == NULL || node == NULL)
        return -1;

    /* perform tilde expansion */
    if(*file == '~')
    {
        char *env = getenv("HOME");
        if(env != NULL)
            snprintf((char*)_file, 1000, "%s%s", env, file + 1);
        else
            snprintf((char*)_file, 1000, "%s", file);
    }
    else
    {
        snprintf((char*)_file, 1000, "%s", file);
    }

    fd = open((char*)&_file, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if(fd < 0)
        return -1;

    doc = xode_to_str(node);
    i = write(fd,doc,strlen(doc));
    if(i < 0)
        return -1;

    close(fd);
    return 1;
}

