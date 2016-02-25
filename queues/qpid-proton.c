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

#ifdef WITH_LIBQPID_PROTON

# define MQ_CONNECTION_STRUCT_DEFINED   1
# define MQ_MESSAGE_STRUCT_DEFINED      1

# include "p_libmq.h"

# include <proton/message.h>
# include <proton/messenger.h>

# define MQ_ERRBUF_LEN                  128

/* MQ implementation members */
static unsigned long mq_proton_release_(MQ *self);
static int mq_proton_error_(MQ *self);
static const char *mq_proton_errmsg_(MQ *self);
static MQSTATE mq_proton_state_(MQ *self);
static int mq_proton_connect_recv_(MQ *self);
static int mq_proton_connect_send_(MQ *self);
static int mq_proton_disconnect_(MQ *self);
static int mq_proton_next_(MQ *self, MQMESSAGE **msg);
static int mq_proton_deliver_(MQ *self);
static int mq_proton_create_(MQ *self, MQMESSAGE **msg);

/* MQMESSAGE implementation members */
static unsigned long mq_proton_message_release_(MQMESSAGE *self);
static MQMSGKIND mq_proton_message_kind_(MQMESSAGE *self);
static int mq_proton_message_accept_(MQMESSAGE *self);
static int mq_proton_message_reject_(MQMESSAGE *self);
static int mq_proton_message_pass_(MQMESSAGE *self);
static int mq_proton_message_send_(MQMESSAGE *self);
static int mq_proton_message_set_type_(MQMESSAGE *self, const char *type);
static const char *mq_proton_message_type_(MQMESSAGE *self);
static int mq_proton_message_set_subject_(MQMESSAGE *self, const char *type);
static const char *mq_proton_message_subject_(MQMESSAGE *self);
static int mq_proton_message_set_address_(MQMESSAGE *self, const char *address);
static const char *mq_proton_message_address_(MQMESSAGE *self);
static const unsigned char *mq_proton_message_body_(MQMESSAGE *self);
static size_t mq_proton_message_len_(MQMESSAGE *self);
static int mq_proton_message_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len);

/* Internal utilities */
static int mq_proton_disconnect_internal_(MQ *self);
static MQMESSAGE *mq_proton_message_construct_(MQ *self);

struct mq_connection_struct
{
	MQCONNIMPL *impl;
	MQ_CONNECTION_COMMON_MEMBERS;
	pn_messenger_t *messenger;
	pn_subscription_t *sub;
	int window_size;
};

struct mq_message_struct
{
	MQMESSAGEIMPL *impl;
	MQ_MESSAGE_COMMON_MEMBERS;
	pn_message_t *msg;
	pn_tracker_t tracker;
	pn_data_t *body;
	pn_bytes_t bytes;
	int addressed:1;
};

static MQCONNIMPL mq_proton_connection_impl_ = {
	/* reserved */
	NULL,
	/* reserved */
	NULL,
	mq_proton_release_,
	mq_proton_error_,
	mq_proton_errmsg_,
	mq_proton_state_,
	mq_proton_connect_recv_,
	mq_proton_connect_send_,
	mq_proton_disconnect_,
	mq_proton_next_,
	mq_proton_deliver_,
	mq_proton_create_
};

static MQMESSAGEIMPL mq_proton_message_impl_ = {
	/* reserved */
	NULL,
	/* reserved */
	NULL,   
	mq_proton_message_release_,
	mq_proton_message_kind_,
	mq_proton_message_accept_,
	mq_proton_message_reject_,
	mq_proton_message_pass_,
	mq_proton_message_send_,
	mq_proton_message_set_type_,
	mq_proton_message_type_,
	mq_proton_message_set_subject_,
	mq_proton_message_subject_,
	mq_proton_message_set_address_,
	mq_proton_message_address_,
	mq_proton_message_body_,
	mq_proton_message_len_,
	mq_proton_message_add_bytes_,
};

/* Proton message queue constructor: this is invoked by libmq to create a new
 * Proton-flavoured MQ instance
 */
MQ *
mq_proton_construct_(const char *uri, const char *reserved1, const char *reserved2)
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
	mq->impl = &mq_proton_connection_impl_;
	mq->uri = p;
	mq->window_size = 1;
	return mq;
}

/* Free an MQ connection object */
static unsigned long
mq_proton_release_(MQ *self)
{
	mq_proton_disconnect_internal_(self);
	free(self->errmsg);
	free(self->uri);
	free(self);
	return 0;
}

/* Return an indicator as to whether the connection is in an error state */
static int
mq_proton_error_(MQ *self)
{
	if(self->errcode || self->syserr)
	{
		return 1;
	}
	return 0;
}

/* Return the error message for the connection */
static const char *
mq_proton_errmsg_(MQ *self)
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
		if(self->messenger)
		{
			return pn_error_text(pn_messenger_error(self->messenger));
		}
		snprintf(self->errmsg, MQ_ERRBUF_LEN, "Unknown error #%d", self->errcode);
		return self->errmsg;
	}
	return "Success";
}

/* Return the MQ connection state */
static MQSTATE
mq_proton_state_(MQ *self)
{
	RESET_ERROR(self);
	return self->state;
}

/* Establish a connection for receiving */
static int
mq_proton_connect_recv_(MQ *self)
{
	int e;

	RESET_ERROR(self);
	if(self->state != MQS_DISCONNECTED)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	self->messenger = pn_messenger(NULL);
	if(!self->messenger)
	{
		SET_ERRNO(self);
		return -1;
	}
	pn_messenger_start(self->messenger);
	if((e = pn_messenger_errno(self->messenger)))
	{
		mq_proton_disconnect_internal_(self);
		SET_ERROR(self, e);
		return 1;
	}
	self->sub = pn_messenger_subscribe(self->messenger, self->uri);
	if(!self->sub || (e = pn_messenger_errno(self->messenger)))
	{
		mq_proton_disconnect_internal_(self);
		SET_ERROR(self, e);
		return 1;
	}
	pn_messenger_set_incoming_window(self->messenger, self->window_size);
	self->state = MQS_RECV;
	return 0;
}

/* Establish a connection for sending */
static int
mq_proton_connect_send_(MQ *self)
{
	int e;

	RESET_ERROR(self);
	if(self->state != MQS_DISCONNECTED)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	self->messenger = pn_messenger(NULL);
	if(!self->messenger)
	{
		SET_ERRNO(self);
		return -1;
	}
	pn_messenger_start(self->messenger);
	if((e = pn_messenger_errno(self->messenger)))
	{
		mq_proton_disconnect_internal_(self);
		SET_ERROR(self, e);
		return 1;
	}
	/* TODO: outgoing window size should be configurable */
	pn_messenger_set_outgoing_window(self->messenger, 1);
	self->state = MQS_SEND;
	return 0;
}

/* Disconnect from a message queue */
static int
mq_proton_disconnect_(MQ *self)
{
	RESET_ERROR(self);
	if(self->state == MQS_DISCONNECTED)
	{
		return 0;
	}
	mq_proton_disconnect_internal_(self);
	RESET_ERROR(self);
	return 0;
}

/* Wait for a message to arrive via a connection */
int
mq_proton_next_(MQ *self, MQMESSAGE **msg)
{
	MQMESSAGE *p;
	int e;

	RESET_ERROR(self);
	if(!pn_messenger_incoming(self->messenger))
	{
		/* There are no buffered incoming messages yet */
		pn_messenger_recv(self->messenger, -1);
		if((e = pn_messenger_errno(self->messenger)))
		{
			SET_ERROR(self, e);
			return -1;
		}
		if(!pn_messenger_incoming(self->messenger))
		{
			/* TODO: if libmq supports non-blocking mode, this should
			 * not be an error
			 */
			SET_ERROR(self, pn_messenger_errno(self->messenger));
			return -1;
		}
	}
	p = mq_proton_message_construct_(self);
	if(!p)
	{
		return -1;
	}
	p->kind = MQK_INCOMING;
	p->msg = pn_message();
	if(!p->msg)
	{
		SET_ERRNO(self);
		free(p);
		return -1;
	}
	pn_messenger_get(self->messenger, p->msg);
	if((e = pn_messenger_errno(self->messenger)))
	{
		pn_message_free(p->msg);
		free(p);
		SET_ERROR(self, e);
		return 1;
	}
	p->tracker = pn_messenger_incoming_tracker(self->messenger);
	p->body = pn_message_body(p->msg);
	if(p->body)
	{
		pn_data_next(p->body);
		p->bytes = pn_data_get_binary(p->body);
	}
	*msg = p;
	return 0;
}

/* Deliver any buffered outgoing messages */
static int
mq_proton_deliver_(MQ *self)
{
	RESET_ERROR(self);
	if(self->state != MQS_SEND)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	if(pn_messenger_send(self->messenger, -1))
	{
		SET_ERROR(self, pn_messenger_errno(self->messenger));
		return -1;
	}
	return 0;
}

/* Create a new outgoing message */
int
mq_proton_create_(MQ *self, MQMESSAGE **msg)
{
	MQMESSAGE *p;

	RESET_ERROR(self);
	p = mq_proton_message_construct_(self);
	if(!p)
	{
		return -1;
	}
	p->kind = MQK_OUTGOING;
	p->msg = pn_message();
	if(!p->msg)
	{
		SET_ERROR(self, pn_messenger_errno(self->messenger));
		free(p);
		return -1;
	}
	*msg = p;
	return 0;
}

/* Release (destroy) a message */
static unsigned long
mq_proton_message_release_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->tracker)
	{
		pn_messenger_settle(self->connection->messenger, self->tracker, 0);
	}
	if(self->msg)
	{
		pn_message_free(self->msg);
	}
	free(self);
	return 0;
}

static MQMSGKIND
mq_proton_message_kind_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return self->kind;
}

/* Mark an incoming message as being accepted */
static int
mq_proton_message_accept_(MQMESSAGE *self)
{   
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	pn_messenger_accept(self->connection->messenger, self->tracker, 0);
	if(self->tracker)
	{
		pn_messenger_settle(self->connection->messenger, self->tracker, 0);
	}
	return 0;
}

static int
mq_proton_message_reject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	pn_messenger_reject(self->connection->messenger, self->tracker, 0);
	if(self->tracker)
	{
		pn_messenger_settle(self->connection->messenger, self->tracker, 0);
	}
	return 0;
}

static int
mq_proton_message_pass_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	if(self->tracker)
	{
		pn_messenger_settle(self->connection->messenger, self->tracker, 0);
	}
	return 0;
}

/* Set the content-type of an outgoing message */
static int
mq_proton_message_set_type_(MQMESSAGE *self, const char *type)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_OUTGOING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return pn_message_set_content_type(self->msg, type);
}

/* Retrieve the content-type of a message */
static const char *
mq_proton_message_type_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(!self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return NULL;
	}
	return pn_message_get_content_type(self->msg);
}

/* Set the subject of a message */
static int
mq_proton_message_set_subject_(MQMESSAGE *self, const char *subject)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_OUTGOING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return pn_message_set_content_type(self->msg, subject);
}

/* Retrieve the content-type of a message */
const char *
mq_proton_message_subject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(!self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return NULL;
	}
	return pn_message_get_subject(self->msg);
}

/* Set the address (destination) of an outgoing message, replacing any
 * previously-set destination
 */
static int
mq_proton_message_set_address_(MQMESSAGE *self, const char *address)
{
	int r;

	RESET_ERROR(self->connection);
	if(self->kind != MQK_OUTGOING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}	
	r = pn_message_set_address(self->msg, address);
	if(!r)
	{
		self->addressed = 1;
	}
	return r;
}

/* Retrieve the address of a message: if it's an outging message, it's the
 * destination; if it's an incoming message, it's the source
 */
static const char *
mq_proton_message_address_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(!self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return NULL;
	}
	return pn_message_get_address(self->msg);
}

/* Retrieve the body of an incoming message */
static const unsigned char *
mq_proton_message_body_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(!self->msg || !self->body)
	{
		SET_SYSERR(self->connection, EINVAL);
		return NULL;
	}
	return (const unsigned char *) self->bytes.start;
}

/* Retrieve the length of an incoming message body, in bytes */
static size_t
mq_proton_message_len_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(!self->msg || !self->body)
	{
		SET_SYSERR(self->connection, EINVAL);
		return (size_t) -1;
	}
	return self->bytes.size;
}

/* Add a sequence of bytes to an outgoing message body */
static int
mq_proton_message_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_OUTGOING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	if(!self->body)
	{
		self->body = pn_message_body(self->msg);
		if(!self->body)
		{
			SET_ERROR(self->connection, pn_messenger_errno(self->connection->messenger));
			return -1;
		}
	}
	if(pn_data_put_binary(self->body, pn_bytes(len, (char *) buf)))
	{
		SET_ERROR(self->connection, pn_messenger_errno(self->connection->messenger));
		return -1;
	}
	return 0;
}

/* Send an outgoing message */
static int
mq_proton_message_send_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_OUTGOING || !self->msg)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	if(!self->addressed)
	{
		if(pn_message_set_address(self->msg, self->connection->uri))
		{
			SET_ERROR(self->connection, pn_messenger_errno(self->connection->messenger));
			return -1;
		}
	}
	if(pn_messenger_put(self->connection->messenger, self->msg))
	{
		SET_ERROR(self->connection, pn_messenger_errno(self->connection->messenger));
		return -1;
	}
	return 0;
}

/* (Internal) create a new MQ message object */
static MQMESSAGE *
mq_proton_message_construct_(MQ *self)
{
	MQMESSAGE *p;

	p = (MQMESSAGE *) calloc(1, sizeof(MQMESSAGE));
	if(!p)
	{
		SET_ERRNO(self);	   
		return NULL;
	}
	p->impl = &mq_proton_message_impl_;
	p->connection = self;
	return p;
}

/* (Internal) disconnect from a message queue */
static int
mq_proton_disconnect_internal_(MQ *self)
{
	if(self->messenger)
	{
		/* TODO; deal with PN_INPROGRESS response from pn_messenger_stop() */
		pn_messenger_stop(self->messenger);
		pn_messenger_free(self->messenger);
		self->messenger = NULL;
		self->sub = NULL;
	}
	self->state = MQS_DISCONNECTED;
	return 0;
}

#endif /*WITH_LIBQPID_PROTON*/
