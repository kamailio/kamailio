/*
 * UAC FIFO interface
 */

#ifndef UAC_FIFO_H
#define UAC_FIFO_H

#include <stdio.h>


/*
 * FIFO function for sending messages
 */
int fifo_uac(FILE *stream, char *response_file);


#endif /* UAC_FIFO_H */
