/* libmq: A library for interacting with message queues
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2015 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_libmq.h"

/* (Internal) create a new disconnected MQ connection object */
MQ *
mq_create_(const char *uri, const char *reserved1, const char *reserved2)
{   
#ifdef WITH_LIBQPID_PROTON
	if(!strncmp(uri, "amqp:", 5) || !strncmp(uri, "amqps:", 6))
	{
		return mq_proton_construct_(uri, reserved1, reserved2);
	}
#endif
	errno = EINVAL;
	return NULL;
}

