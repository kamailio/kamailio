#ifndef _FIFO_T_REPLY_H_
#define _FIFO_T_REPLY_H_

#include <stdio.h> // just for FILE* ...

/*
  Syntax:

  :vm_reply:[file] EOL
  code EOL
  reason EOL
  trans_id EOL
  to_tag EOL
  [new headers]
  .EOL
  [Body] EOL
  .EOL
  EOL
 */

int fifo_vm_reply( FILE* stream, char *response_file );

#endif


