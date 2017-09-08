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

int
mq_plugin_init_(void)
{
	int verbose = !!(getenv("MQ_PLUGIN_DEBUG"));
	DIR *dir;
	struct dirent direntry, *de;
	const char *t;
	char *buf, *p;
	size_t buflen, baselen, namelen;
	void *handle;
	MQENTRY entry;

	if(verbose)
	{
		fprintf(stderr, "MQ: loading plug-ins from %s\n", PLUGINDIR);
	}
	buf = NULL;
	buflen = 0;
	baselen = strlen(PLUGINDIR);   
	dir = opendir(PLUGINDIR);
	if(!dir)
	{
		if(errno != ENOENT || verbose)
		{
			fprintf(stderr, "MQ: cannot open '%s': %s\n", PLUGINDIR, strerror(errno));
			return (errno == ENOENT ? 0 : -1);
		}
	}
	while(!readdir_r(dir, &direntry, &de) && de)
	{
		if(de->d_name[0] == '.')
		{
			continue;
		}
		if(!(t = strrchr(de->d_name, '.')))
		{
			continue;
		}
		if(!strcasecmp(t, ".so") ||
		   !strcasecmp(t, ".dylib") ||
		   !strcasecmp(t, ".bundle") ||
		   !strcasecmp(t, ".dll"))
		{
			namelen = baselen + strlen(de->d_name) + 2;
			if(verbose)
			{
				fprintf(stderr, "MQ: loading plug-in %s\n", de->d_name);
			}
			if(namelen > buflen)
			{
				p = (char *) realloc(buf, namelen);
				if(!p)
				{
					fprintf(stderr, "MQ: failed to allocate pathname buffer\n");
					free(buf);
					closedir(dir);
					return -1;
				}
				if(!buf)
				{
					strncpy(p, PLUGINDIR, namelen);
					p[baselen] = '/';
				}
				buf = p;
				buflen = namelen;
			}
			buf[baselen + 1] = 0;
			strncat(buf, de->d_name, namelen);
			handle = dlopen(buf, RTLD_NOW|RTLD_LOCAL);
			if(!handle)
			{
				fprintf(stderr, "MQ: failed to load %s: %s\n", buf, dlerror());
				continue;
			}
			entry = dlsym(handle, "mq_entry");
			if(!entry)
			{
				fprintf(stderr, "MQ: %s has no initialisation function\n", buf);
				dlclose(handle);
				continue;
			}
			if(entry(handle))
			{
				fprintf(stderr, "MQ: %s: initialisation failed\n", buf);
				dlclose(handle);
				continue;
			}
			if(verbose)
			{
				fprintf(stderr, "MQ: %s initialised\n", buf);
			}
		}
	}
	free(buf);
	closedir(dir);
	return 0;
}

