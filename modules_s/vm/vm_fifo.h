#ifndef _FIFO_T_REPLY_H_
#define _FIFO_T_REPLY_H_

//#include "h_table.h"
//#include "../../parser/msg_parser.h"

#include <stdio.h> // just for FILE* ...

/*
  Syntax:

  :vm_reply:[file] EOL
  code EOL
  reason EOL
  trans_id EOL
  to_tag EOL
  [new headers] EOL
  EOL
  [Body] EOL
  .EOL
  EOL
 */

int fifo_vm_reply( FILE* stream, char *response_file );


/* syntax:

	:t_uac_dlg:[file] EOL
	method EOL
	dst EOL
	[r-uri] EOL (if none, dst is taken)
	[to;[tag=to_tag]] EOL (if no 'to', dst is taken)
	[from;[tag=from_tag]] EOL (if no 'from', server's default from is taken)
	[cseq] EOL
	[call_id] EOL
	[CR-LF separated HFs]* EOL
	EOL
	[body] EOL
	.EOL
	EOL
*/

int fifo_uac_dlg( FILE *stream, char *response_file );

#endif


