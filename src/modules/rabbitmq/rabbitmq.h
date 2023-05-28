/*
 * MIT License
 *
 * Portions created by ONEm Communications Ltd. are Copyright (c) 2016
 * ONEm Communications Ltd. All Rights Reserved.
 *
 * Portions created by ng-voice are Copyright (c) 2016
 * ng-voice. All Rights Reserved.
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _RABBITMQ_H_
#define _RABBITMQ_H_

#define RABBITMQ_DEFAULT_AMQP_URL "amqp://guest:guest@localhost:5672/%2F"

typedef enum
{
	RABBITMQ_OK = 1,
	RABBITMQ_ERR_CONNECT,
	RABBITMQ_ERR_CHANNEL,
	RABBITMQ_ERR_QUEUE,
	RABBITMQ_ERR_PUBLISH,
	RABBITMQ_ERR_SOCK,
	RABBITMQ_ERR_CONSUME,
	RABBITMQ_ERR_NULL,
} RABBITMQ_ENUM;

#endif
