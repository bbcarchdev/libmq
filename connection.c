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

/* Create a connection for receiving messages from a queue */
MQ *
mq_connect_recv(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;

	mq = mq_create_(uri, reserved1, reserved2);
	if(!mq)
	{
		return NULL;
	}
	if(mq->impl->connect_recv(mq))
	{
		mq->impl->release(mq);
		return NULL;
	}
	return mq;
}

/* Create a connection for receiving messages from a queue */
MQ *
mq_connect_send(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;

	mq = mq_create_(uri, reserved1, reserved2);
	if(!mq)
	{
		return NULL;
	}
	if(mq->impl->connect_send(mq))
	{
		mq->impl->release(mq);
		return NULL;
	}
	return mq;
}

/* Close a connection */
int
mq_disconnect(MQ *connection)
{
	connection->impl->release(connection);
	return 0;
}

/* Set the cluster */
int
mq_set_cluster(MQ *connection, CLUSTER *cluster)
{
	connection->impl->set_cluster(connection, cluster);
	return 0;
}

/* Create a new outgoing message */
MQMESSAGE *
mq_message_create(MQ *connection)
{
	MQMESSAGE *message;

	message = NULL;
	if(connection->impl->create(connection, &message))
	{
		return NULL;
	}
	return message;
}

/* Wait for the next message to arrive */
MQMESSAGE *
mq_next(MQ *connection)
{
	MQMESSAGE *message;

	message = NULL;
	if(connection->impl->next(connection, &message))
	{
		return NULL;
	}
	return message;
}

/* Deliver any outgoing messages */
int
mq_deliver(MQ *connection)
{
	return connection->impl->deliver(connection);
}

/* Obtain the error state for a connection */
int
mq_error(MQ *connection)
{
	return connection->impl->error(connection);
}

/* Obtain the error message for a connection */
const char *
mq_errmsg(MQ *connection)
{
	return connection->impl->errmsg(connection);
}

