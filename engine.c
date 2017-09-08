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

# ifdef WITH_LIBQPID_PROTON
MQ *mq_proton_construct_(const char *uri, const char *reserved1, const char *reserved2);
# endif

struct mq_engine_struct
{
	char *scheme;
	MQCONSTRUCTOR construct;
	void *handle;
};

static void mq_init_(void);
static int mq_register_internal_(const char *scheme, MQCONSTRUCTOR construct, void *handle);

static pthread_once_t mq_init_once_ = PTHREAD_ONCE_INIT;

static pthread_rwlock_t enginelock = PTHREAD_RWLOCK_INITIALIZER;
static struct mq_engine_struct *engines;
static size_t enginecount;

/* Register an MQ engine. handle is intended to be a module handle as returned
 * by dlopen(), or some similar kind of application-specific pointer; it's
 * passed to mq_unregister() to ensure that modules don't inadvertently
 * unregister each other's handlers, and can be used with mq_unregister_all()
 * to remove any handlers registered to a particular module handle.
 */
int
mq_register(const char *scheme, MQCONSTRUCTOR construct, void *handle)
{
	return mq_register_internal_(scheme, construct, handle);
}

/* Un-register a previously-registered scheme */
int
mq_unregister(const char *scheme, void *handle)
{
	size_t c;

	pthread_rwlock_wrlock(&enginelock);
	for(c = 0; c < enginecount; c++)
	{
		if(!engines[c].scheme)
		{
			continue;
		}
		if(!strcmp(engines[c].scheme, scheme) &&
		   engines[c].handle == handle)
		{
			free(engines[c].scheme);
			memset(&(engines[c]), 0, sizeof(struct mq_engine_struct));
			pthread_rwlock_unlock(&enginelock);
			return 1;
		}
	}
	pthread_rwlock_unlock(&enginelock);
	return 0;
}

/* Un-register any schemes using a particular constructor function */
int
mq_unregister_constructor(MQCONSTRUCTOR construct)
{
	size_t c;
	int count;

	pthread_rwlock_wrlock(&enginelock);
	count = 0;
	for(c = 0; c < enginecount; c++)
	{
		if(!engines[c].scheme)
		{
			continue;
		}
		if(engines[c].construct == construct)
		{
			free(engines[c].scheme);
			memset(&(engines[c]), 0, sizeof(struct mq_engine_struct));
			count++;
		}
	}
	pthread_rwlock_unlock(&enginelock);
	return count;
}

/* Un-register any schemes associated with a particular module handle */
int
mq_unregister_all(void *handle)
{
	size_t c;
	int count;

	pthread_rwlock_wrlock(&enginelock);
	count = 0;
	for(c = 0; c < enginecount; c++)
	{
		if(!engines[c].scheme)
		{
			continue;
		}
		if(engines[c].handle == handle)
		{
			free(engines[c].scheme);
			memset(&(engines[c]), 0, sizeof(struct mq_engine_struct));
			count++;
		}
	}
	pthread_rwlock_unlock(&enginelock);
	return count;
}

/* (Internal) create a new disconnected MQ connection object */
MQ *
mq_create_(const char *uri, const char *reserved1, const char *reserved2)
{
	size_t c, l;
	MQ *mq;

	pthread_once(&mq_init_once_, mq_init_);
	pthread_rwlock_rdlock(&enginelock);
	for(c = 0; c < enginecount; c++)
	{
		if(!engines[c].scheme)
		{
			continue;
		}
		l = strlen(engines[c].scheme);
		if(strlen(uri) > l &&
		   !strncmp(uri, engines[c].scheme, l) &&
		   uri[l] == ':')
		{
			mq = engines[c].construct(uri, reserved1, reserved2);
			pthread_rwlock_unlock(&enginelock);
			return mq;
		}
	}
	pthread_rwlock_unlock(&enginelock);
	errno = EINVAL;
	return NULL;
}

/* (Internal) initialise the list of MQ engines */
static void
mq_init_(void)
{
#ifdef WITH_LIBQPID_PROTON
	mq_register_internal_("amqp", mq_proton_construct_, NULL);
	mq_register_internal_("amqps", mq_proton_construct_, NULL);
#endif
	mq_plugin_init_();
}

/* (Internal) register an engine */
static int
mq_register_internal_(const char *scheme, MQ *(*construct)(const char *, const char *, const char *), void *handle)
{
	size_t c, spare;
	struct mq_engine_struct *p;
	char *s;

	pthread_rwlock_wrlock(&enginelock);
	spare = (size_t) -1;
	for(c = 0; c < enginecount; c++)
	{
		if(!engines[c].scheme)
		{
			spare = c;
			continue;
		}
		if(!strcmp(engines[c].scheme, scheme))
		{
			engines[c].construct = construct;
			engines[c].handle = handle;
			pthread_rwlock_unlock(&enginelock);
			return 0;
		}
	}
	s = strdup(scheme);
	if(!s)
	{
		pthread_rwlock_unlock(&enginelock);
		return -1;
	}
	if(spare == (size_t) -1)
	{
		p = (struct mq_engine_struct *) realloc(engines, sizeof(struct mq_engine_struct) * (enginecount + 1));
		if(!p)
		{
			pthread_rwlock_unlock(&enginelock);
			return -1;
		}
		engines = p;
		p = &(engines[enginecount]);
		enginecount++;
	}
	else
	{
		p = &(engines[spare]);
	}
	p->scheme = s;
	p->construct = construct;
	p->handle = handle;
	pthread_rwlock_unlock(&enginelock);
	return 0;
}
