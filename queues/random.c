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

#define MQ_CONNECTION_STRUCT_DEFINED   1
#define MQ_MESSAGE_STRUCT_DEFINED      1

#include "p_libmq.h"

#include <time.h>

#define MQ_ERRBUF_LEN                  128

/* MQ implementation members */
static unsigned long mq_random_release_(MQ *self);
static int mq_random_error_(MQ *self);
static const char *mq_random_errmsg_(MQ *self);
static MQSTATE mq_random_state_(MQ *self);
static int mq_random_connect_recv_(MQ *self);
static int mq_random_connect_send_(MQ *self);
static int mq_random_disconnect_(MQ *self);
static int mq_random_next_(MQ *self, MQMESSAGE **msg);
static int mq_random_deliver_(MQ *self);
static int mq_random_create_(MQ *self, MQMESSAGE **msg);

/* MQMESSAGE implementation members */
static unsigned long mq_random_message_release_(MQMESSAGE *self);
static MQMSGKIND mq_random_message_kind_(MQMESSAGE *self);
static int mq_random_message_accept_(MQMESSAGE *self);
static int mq_random_message_reject_(MQMESSAGE *self);
static int mq_random_message_pass_(MQMESSAGE *self);
static int mq_random_message_send_(MQMESSAGE *self);
static int mq_random_message_set_type_(MQMESSAGE *self, const char *type);
static const char *mq_random_message_type_(MQMESSAGE *self);
static int mq_random_message_set_subject_(MQMESSAGE *self, const char *type);
static const char *mq_random_message_subject_(MQMESSAGE *self);
static int mq_random_message_set_address_(MQMESSAGE *self, const char *address);
static const char *mq_random_message_address_(MQMESSAGE *self);
static const unsigned char *mq_random_message_body_(MQMESSAGE *self);
static size_t mq_random_message_len_(MQMESSAGE *self);
static int mq_random_message_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len);

struct mq_connection_struct
{
	MQCONNIMPL *impl;
	MQ_CONNECTION_COMMON_MEMBERS;
};

struct mq_message_struct
{
	MQMESSAGEIMPL *impl;
	MQ_MESSAGE_COMMON_MEMBERS;
	char unsigned buf[32];
};

static MQCONNIMPL mq_random_connection_impl_ = {
	/* reserved */
	NULL,
	/* reserved */
	NULL,
	mq_random_release_,
	mq_random_error_,
	mq_random_errmsg_,
	mq_random_state_,
	mq_random_connect_recv_,
	mq_random_connect_send_,
	mq_random_disconnect_,
	mq_random_next_,
	mq_random_deliver_,
	mq_random_create_
};

static MQMESSAGEIMPL mq_random_message_impl_ = {
	/* reserved */
	NULL,
	/* reserved */
	NULL,   
	mq_random_message_release_,
	mq_random_message_kind_,
	mq_random_message_accept_,
	mq_random_message_reject_,
	mq_random_message_pass_,
	mq_random_message_send_,
	mq_random_message_set_type_,
	mq_random_message_type_,
	mq_random_message_set_subject_,
	mq_random_message_subject_,
	mq_random_message_set_address_,
	mq_random_message_address_,
	mq_random_message_body_,
	mq_random_message_len_,
	mq_random_message_add_bytes_,
};

static MQMESSAGE *mq_random_message_construct_(MQ *self);

/* Random message queue constructor: this is invoked by libmq to create a new
 * Random-flavoured MQ instance
 */
MQ *
mq_random_construct_(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;
	char *p;

	(void) reserved1;
	(void) reserved2;
	
	mq = (MQ *) calloc(1, sizeof(MQ));
	if(!mq)
	{
		return NULL;
	}
	p = strdup(uri);
	if(!p)
	{
		free(mq);
		return NULL;
	}
	mq->impl = &mq_random_connection_impl_;
	mq->uri = p;
	return mq;
}

/* Free an MQ connection object */
static unsigned long
mq_random_release_(MQ *self)
{
	free(self->errmsg);
	free(self->uri);
	free(self);
	return 0;
}

/* Return an indicator as to whether the connection is in an error state */
static int
mq_random_error_(MQ *self)
{
	if(self->errcode || self->syserr)
	{
		return 1;
	}
	return 0;
}

/* Return the error message for the connection */
static const char *
mq_random_errmsg_(MQ *self)
{
	if(!self->errmsg)
	{
		self->errmsg = (char *) malloc(MQ_ERRBUF_LEN);
		if(!self->errmsg)
		{
			return "Memory allocation error obtaining error message";
		}
	}	
	self->errmsg[0] = 0;
	if(self->syserr)
	{
		strerror_r(self->syserr, self->errmsg, MQ_ERRBUF_LEN);
		return self->errmsg;
	}
	if(self->errcode)
	{
		snprintf(self->errmsg, MQ_ERRBUF_LEN, "Unknown error #%d", self->errcode);
		return self->errmsg;
	}
	return "Success";
}

/* Return the MQ connection state */
static MQSTATE
mq_random_state_(MQ *self)
{
	RESET_ERROR(self);
	return self->state;
}

/* Establish a connection for receiving */
static int
mq_random_connect_recv_(MQ *self)
{
	RESET_ERROR(self);
	if(self->state != MQS_DISCONNECTED)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	srandom((unsigned int) time(NULL));
	self->state = MQS_RECV;
	return 0;
}

/* Establish a connection for sending */
static int
mq_random_connect_send_(MQ *self)
{
	/* You can't send a message to this queue handler */
	SET_SYSERR(self, EPERM);
	return -1;
}

/* Disconnect from a message queue */
static int
mq_random_disconnect_(MQ *self)
{
	RESET_ERROR(self);
	self->state = MQS_DISCONNECTED;
	return 0;
}

/* Wait for a message to arrive via a connection */
int
mq_random_next_(MQ *self, MQMESSAGE **msg)
{
	MQMESSAGE *p;

	p = mq_random_message_construct_(self);
	if(!p)
	{
		return -1;
	}
	p->kind = MQK_INCOMING;
	snprintf((char *) p->buf, sizeof(p->buf), "%ld", random());
	*msg = p;
	return 0;
}

/* Deliver any buffered outgoing messages */
static int
mq_random_deliver_(MQ *self)
{
	RESET_ERROR(self);
	if(self->state != MQS_SEND)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	return 0;
}

/* Create a new outgoing message */
int
mq_random_create_(MQ *self, MQMESSAGE **msg)
{
	/* This engine can't create outgoing messages */
	(void) msg;

	SET_SYSERR(self, EINVAL);
	return -1;
}

/* Release (destroy) a message */
static unsigned long
mq_random_message_release_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	free(self);
	return 0;
}

static MQMSGKIND
mq_random_message_kind_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return self->kind;
}

/* Mark an incoming message as being accepted */
static int
mq_random_message_accept_(MQMESSAGE *self)
{   
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

static int
mq_random_message_reject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

static int
mq_random_message_pass_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

/* Set the content-type of an outgoing message */
static int
mq_random_message_set_type_(MQMESSAGE *self, const char *type)
{
	(void) type;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the content-type of a message */
static const char *
mq_random_message_type_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return NULL;
}

/* Set the subject of a message */
static int
mq_random_message_set_subject_(MQMESSAGE *self, const char *subject)
{
	(void) subject;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the content-type of a message */
const char *
mq_random_message_subject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);

	return NULL;
}

/* Set the address (destination) of an outgoing message, replacing any
 * previously-set destination
 */
static int
mq_random_message_set_address_(MQMESSAGE *self, const char *address)
{
	(void) address;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the address of a message: if it's an outging message, it's the
 * destination; if it's an incoming message, it's the source
 */
static const char *
mq_random_message_address_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return "random:";
}

/* Retrieve the body of an incoming message */
static const unsigned char *
mq_random_message_body_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return self->buf;
}

/* Retrieve the length of an incoming message body, in bytes */
static size_t
mq_random_message_len_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return strlen((const char *) self->buf);
}

/* Add a sequence of bytes to an outgoing message body */
static int
mq_random_message_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len)
{
	(void) buf;
	(void) len;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Send an outgoing message */
static int
mq_random_message_send_(MQMESSAGE *self)
{
	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* (Internal) create a new MQ message object */
static MQMESSAGE *
mq_random_message_construct_(MQ *self)
{
	MQMESSAGE *p;

	p = (MQMESSAGE *) calloc(1, sizeof(MQMESSAGE));
	if(!p)
	{
		SET_ERRNO(self);	   
		return NULL;
	}
	p->impl = &mq_random_message_impl_;
	p->connection = self;
	return p;
}
