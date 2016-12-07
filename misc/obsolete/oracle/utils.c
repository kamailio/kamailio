/*
 * Copyright (C) 2005 RingCentral Inc.
 * Created by Dmitry Semyonov <dsemyonov@ringcentral.com>
 *
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <oci.h>

#include "../../dprint.h"

#include "common.h"
#include "utils.h"


sb4 checkerr(dvoid *errhp, sword status, int line)
{
  sb4 errcode = 0;

  if (status == OCI_SUCCESS)
    return 0;

#define CASE_LOG_STR(_err) \
  case _err:                        \
    ERR("Error - " #_err);   \
    break

  switch (status)
  {
    CASE_LOG_STR(OCI_SUCCESS_WITH_INFO);
    CASE_LOG_STR(OCI_NEED_DATA);
    CASE_LOG_STR(OCI_NO_DATA);
    CASE_LOG_STR(OCI_INVALID_HANDLE);
    CASE_LOG_STR(OCI_STILL_EXECUTING);
    CASE_LOG_STR(OCI_CONTINUE);

    case OCI_ERROR:
    {
      text errbuf[512];
      errbuf[0] = '\0';
  
      (void) OCIErrorGet(errhp, 1, NULL, &errcode, errbuf, sizeof(errbuf),
                         OCI_HTYPE_ERROR);
  
      /* Special case. (Search 24334 in dbase.c for explanation.) */
      if (errcode == 24334)
      {
        DBG1("ORA-24334. Most likely not an error.\n");
        return 24334;
      }
  
      ERR("Error - %.*s", sizeof(errbuf), errbuf);
      break;
    }
  
    default:
      ERR("Error - unhandled status: %d", status);
  }

  ERR(". Line %d\n", line);
  
  return errcode;
}

const char *sqlt_to_str(ub2 sqlt)
{
#define CASE_STR(_val) case _val: return #_val

  switch (sqlt)
  {
    CASE_STR(SQLT_CHR);
    CASE_STR(SQLT_STR);
    CASE_STR(SQLT_INT);
    CASE_STR(SQLT_UIN);
    CASE_STR(SQLT_FLT);
    CASE_STR(SQLT_DAT);

    CASE_STR(SQLT_DATE);
    CASE_STR(SQLT_TIMESTAMP);
    CASE_STR(SQLT_TIMESTAMP_TZ);
    CASE_STR(SQLT_TIMESTAMP_LTZ);
    CASE_STR(SQLT_NUM);

    default:
      return "Unknown SQLT_xxx";
  }
}

/*
 * URL example: "oracle://serro:47serro11@localhost:port/ser"
 * XXX: Buffer lengths are not checked yet!
 */
int parse_sql_url(const char *_sqlurl, char *sz_user, char *sz_passwd,
                  char *sz_db)
{
  /* Skip 'oracle://' part */

  while ( *_sqlurl != '\0' && *_sqlurl++ != ':' );

  if ( *_sqlurl++ != '/' || *_sqlurl++ != '/' )
    return -1;


  /* Get username */

  do {
    *sz_user++ = *_sqlurl++;
  } while ( *_sqlurl != ':' && *_sqlurl != '\0' );

  if ( *_sqlurl++ != ':' )
    return -2;
  
  *sz_user = '\0';
  

  /* Get password */

  do {
    *sz_passwd++ = *_sqlurl++;
  } while ( *_sqlurl != '@' && *_sqlurl != '\0' );

  if ( *_sqlurl++ != '@' )
    return -3;

  *sz_passwd = '\0';
  

  /* Skip 'localhost:port' part */

  while ( *_sqlurl != '\0' && *_sqlurl++ != '/' );

  if ( *_sqlurl == '\0' )
    return -4;


  /* Get db */

  do {
    *sz_db++ = *_sqlurl++;
  } while ( *_sqlurl != '\0' );
  
  *sz_db = '\0';
  
  
  return 0;
}  
