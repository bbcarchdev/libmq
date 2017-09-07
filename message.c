/* libmq: A library for interacting with message queues
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

/* Determine the kind of a message */
MQMSGKIND
mq_message_kind(MQMESSAGE *message)
{
	return message->impl->kind(message);
}

/* Accept and free a message */
int
mq_message_accept(MQMESSAGE *message)
{
	int r;

	r = message->impl->accept(message);
	message->impl->release(message);
	return r;
}

/* Reject and free a message */
int
mq_message_reject(MQMESSAGE *message)
{
	int r;

	r = message->impl->reject(message);
	message->impl->release(message);
	return r;
}

/* Pass on and free a message */
int
mq_message_pass(MQMESSAGE *message)
{
	int r;

	r = message->impl->pass(message);
	message->impl->release(message);
	return r;
}

/* Free a message */
int
mq_message_free(MQMESSAGE *message)
{
	message->impl->release(message);
	return 0;
}

/* Set the content type of a message */
int
mq_message_set_type(MQMESSAGE *message, const char *type)
{	
	return message->impl->set_type(message, type);
}

/* Return the content type of a message */
const char *
mq_message_type(MQMESSAGE *message)
{	
	return message->impl->type(message);
}

/* Set the content type of a message */
int
mq_message_set_subject(MQMESSAGE *message, const char *subject)
{
	return message->impl->set_subject(message, subject);
}

/* Return the subject of a message */
const char *
mq_message_subject(MQMESSAGE *message)
{
	return message->impl->subject(message);
}

/* Set the destination address of a message */
int
mq_message_set_address(MQMESSAGE *message, const char *address)
{
	return message->impl->set_address(message, address);
}

/* Return the address of a message */
const char *
mq_message_address(MQMESSAGE *message)
{
	return message->impl->address(message);
}

/* Return the message body */
const unsigned char *
mq_message_body(MQMESSAGE *message)
{
	return message->impl->body(message);
}

/* Return the length of the message body */
size_t
mq_message_len(MQMESSAGE *message)
{
	return message->impl->len(message);
}

/* Add bytes to the message body */
int
mq_message_add_bytes(MQMESSAGE *message, unsigned char *bytes, size_t len)
{	
	return message->impl->add_bytes(message, bytes, len);
}

/* Override the queue's partition for an individual message */
int
mq_message_set_partition(MQMESSAGE *message, const char *partition)
{
	return message->impl->set_partition(message, partition);
}

/* Obtain the message partition, if any */
const char *
mq_message_partition(MQMESSAGE *message)
{
	return message->impl->partition(message);
}

/* Send a message */
int
mq_message_send(MQMESSAGE *message)
{	
	return message->impl->send(message);
}
