## libmq: A library for interacting with message queues
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2015 BBC
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libmq.la

include_HEADERS = libmq.h libmq-engine.h

bin_PROGRAMS = mq-recv mq-send

EXTRA_DIST = libmq.pc.in libmq-uninstalled.pc.in

DISTCLEANFILES = libmq.pc libmq-uninstalled.pc

SUBDIRS = queues m4

pkgconfigdir = $(libdir)/pkgconfig

noinst_DATA = libmq-uninstalled.pc

pkgconfig_DATA = libmq.pc

libmq_la_SOURCES = p_libmq.h \
	engine.c connection.c message.c plugin.c

libmq_la_LDFLAGS = -avoid-version

libmq_la_LIBADD = queues/libqueues.la

mq_recv_SOURCES = mq-recv.c
mq_recv_LDADD = libmq.la

mq_send_SOURCES = mq-send.c
mq_send_LDADD = libmq.la

BRANCH ?= develop

DEVELOP_SUBMODULES = m4

checkout:
	@for submod in $(DEVELOP_SUBMODULES) ; do \
		echo "+++ Checking out the latest version of $$submod on $(BRANCH)" >&2 ; \
		cd $$submod && \
		git checkout $(BRANCH) && \
		git pull origin $(BRANCH) && \
		$(MAKE) checkout BRANCH=$(BRANCH) || exit $? && \
		cd .. ; \
	done

reconf:
	autoreconf -i && $(SHELL) ./config.status --recheck && $(SHELL) ./config.status


