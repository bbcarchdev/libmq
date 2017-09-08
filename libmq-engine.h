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

#ifndef LIBMQ_ENGINE_H_
# define LIBMQ_ENGINE_H_                1

# include <libmq.h>

# undef BEGIN_DECLS_
# undef END_DECLS_
# ifdef __cplusplus
#  define BEGIN_DECLS_                  extern "C" {
#  define END_DECLS_                    }
# else
#  define BEGIN_DECLS_
#  define END_DECLS_
# endif

BEGIN_DECLS_;

typedef struct mq_connection_impl_struct MQCONNIMPL;
typedef struct mq_message_impl_struct MQMESSAGEIMPL;

typedef int (*MQENTRY)(void *self);

typedef MQ *(*MQCONSTRUCTOR)(const char *uri, const char *reserved1, const char *reserved2);

/* Define a generic MQ structure. Individual implementations should define
 * MQ_CONNECTION_STRUCT_DEFINED before including this file and declare their
 * own struct mq_connection_struct, ensuring the first member is a pointer to
 * a MQCONNIMPL, which is defined below.
 *
 * Any other members of this structure are defined by, and private to, the
 * actual engine. However, if the structure includes the
 * MQ_CONNECTION_COMMON_MEMBERS macro, then convenience macros for
 * error-handling defined by this file can be used by the engine.
 */

# define MQ_CONNECTION_COMMON_MEMBERS \
	MQSTATE state; \
	int syserr; \
	int errcode; \
	char *errmsg; \
	char *uri; \
	CLUSTER *cluster;

# ifndef MQ_CONNECTION_STRUCT_DEFINED
struct mq_connection_struct
{
	MQCONNIMPL *impl;
};
# endif /*!MQ_CONNECTION_STRUCT_DEFINED*/

/* Define a generic MQ message structure. Individual implementations should
 * define MQ_MESSAGE_STRUCT_DEFINED before including this file and declare
 * their own struct mq_message_struct, ensuring the first member is a pointer
 * to an implementation-provided MQMESSAGEIMPL, which is declared below.
 *
 * Any other members of this structure are defined by, and private to, the
 * actual engine. However, if the structure includes the
 * MQ_MESSAGE_COMMON_MEMBERS macro, then convenience macros defined by this
 * file can be used by the engine.
 */

# define MQ_MESSAGE_COMMON_MEMBERS \
	MQ *connection; \
	MQMSGKIND kind;

# ifndef MQ_MESSAGE_STRUCT_DEFINED
struct mq_message_struct
{
	MQMESSAGEIMPL *impl;
};
# endif /*!MQ_MESSAGE_STRUCT_DEFINED*/

/* Reset the error state on a connection */
# define RESET_ERROR(conn) \
	conn->syserr = conn->errcode = 0;

/* Set an implementation-defined error code on the connection */
# define SET_ERROR(conn, code) \
	conn->syserr = 0; \
	conn->errcode = code;

/* Set a system (errno) error code on the connection and errno */
# define SET_SYSERR(conn, value) \
	conn->errcode = 0; \
	conn->syserr = errno = value;

/* Set the system error state on the connection to be the value of errno */
# define SET_ERRNO(conn) \
	conn->errcode = 0; \
	conn->syserr = errno;

struct mq_connection_impl_struct
{
	/* These members should be set to NULL */
	void *reserved1;
	void *reserved2;
	/* Release (destroy) the connection */
	unsigned long (*release)(MQ *self);
	/* Return the error state of the connection */
	int (*error)(MQ *self);
	/* Return the error message for the connection */
	const char *(*errmsg)(MQ *self);
	/* Obtain the connection state (MQS_*) */
	MQSTATE (*state)(MQ *self);
	/* Initialise the connection for receiving messages */
	int (*connect_recv)(MQ *self);
	/* Initialise the connection for sending messages */
	int (*connect_send)(MQ *self);
	/* Disconnect from the queue */
	int (*disconnect)(MQ *self);
	/* Wait for the next message to arrive */
	int (*next)(MQ *self, MQMESSAGE **msg);
	/* Deliver any buffered outgoing messages */
	int (*deliver)(MQ *self);
	/* Create a new outbound message */
	int (*create)(MQ *self, MQMESSAGE **msg);
	/* Set the cluster that this queue is attached to */
	int (*set_cluster)(MQ *self, CLUSTER *cluster);
	/* Obtain the cluster that this queue is attached to, if any */
	CLUSTER *(*cluster)(MQ *self);
	/* Set the partition that this queue is associated with */
	int (*set_partition)(MQ *self, const char *partition);
	/* Obtain the partition that this queue is associated with */
	const char *(*partition)(MQ *self);
};

struct mq_message_impl_struct
{
	/* These members should be set to NULL */
	void *reserved1;
	void *reserved2;
	/* Release (destroy) the message */
	unsigned long (*release)(MQMESSAGE *self);
	/* Obtain the message kind (MQK_*) */
	MQMSGKIND (*kind)(MQMESSAGE *self);
	/* Accept an incoming message */
	int (*accept)(MQMESSAGE *self);
	/* Reject an incoming message */
	int (*reject)(MQMESSAGE *self);
	/* Pass an incoming message */
	int (*pass)(MQMESSAGE *self);
	/* Send an outgoing message */
	int (*send)(MQMESSAGE *self);
	/* Set the content type of an outgoing message */
	int (*set_type)(MQMESSAGE *self, const char *type);
	/* Retrieve the content type of a message */
	const char *(*type)(MQMESSAGE *self);
	/* Set the subject of an outgoing message */
	int (*set_subject)(MQMESSAGE *self, const char *subject);
	/* Retrieve the subject of a message */
	const char *(*subject)(MQMESSAGE *self);
	/* Set the destination for a message */
	int (*set_address)(MQMESSAGE *self, const char *destination);
	/* Retrieve the address (source or destination) of a message */
	const char *(*address)(MQMESSAGE *self);
	/* Retrieve a message body */
	const unsigned char *(*body)(MQMESSAGE *self);
	/* Retrieve the message body length (in bytes) */
	size_t (*len)(MQMESSAGE *self);
	/* Add a sequence of bytes to an outgoing message */
	int (*add_bytes)(MQMESSAGE *self, unsigned char *buf, size_t buflen);
	/* Set the partition that this message is associated with */
	int (*set_partition)(MQMESSAGE *self, const char *partition);
	/* Obtain the partition that this message is associated with */
	const char *(*partition)(MQMESSAGE *self);	
};

int mq_register(const char *scheme, MQCONSTRUCTOR construct, void *handle);
int mq_unregister(const char *scheme, void *handle);
int mq_unregister_constructor(MQCONSTRUCTOR construct);
int mq_unregister_all(void *handle);

/* Forward declaration for the plug-in entry-point */

int mq_entry(void *self);

END_DECLS_;

#endif /*!LIBMQ_ENGINE_H_*/
