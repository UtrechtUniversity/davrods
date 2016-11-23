# Davrods build system.
# Author: Chris Smeele
# Copyright (c) 2016, Utrecht University

MODNAME      ?= davrods
SHARED_FNAME := mod_$(MODNAME).so
SHARED       := ./.libs/$(SHARED_FNAME)

# XXX: These are currently unused as we rely on the apxs utility for module
#      installation.
INSTALL_DIR  ?= /usr/lib64/httpd/modules
INSTALLED    := $(INSTALL_DIR)/mod_$(MODNAME).so

CFILES := mod_davrods.c auth.c common.c config.c prop.c propdb.c repo.c
HFILES := mod_davrods.h auth.h common.h config.h prop.h propdb.h repo.h

# The DAV providers supported by default (you can override this in the shell using DAV_PROVIDERS="..." make).
DAV_PROVIDERS ?= LOCALLOCK NOLOCKS

# A string that's prepended to davrods's DAV provider names (<string>-nolocks, <string>-locallock, ...).
DAV_PROVIDER_NAME_PREFIX    ?= $(MODNAME)

# A string that's prepended to davrods's configuration directives (<string>Zone, <string>DefaultResource, ...).
# Note: This is NOT case-sensitive.
DAV_CONFIG_DIRECTIVE_PREFIX ?= $(MODNAME)

ifneq (,$(findstring LOCALLOCK,$(DAV_PROVIDERS)))
# Compile local locking support using DBM if requested.
CFILES += lock_local.c
HFILES += lock_local.h
endif

SRCFILES := $(CFILES) $(HFILES)
OUTFILES := $(CFILES:%.c=%.o) $(CFILES:%.c=%.lo) $(CFILES:%.c=%.slo) $(CFILES:%.c=%.la)

# Most of these are iRODS client lib dependencies.
LIBS :=              \
	dl               \
	m                \
	pthread          \
	ssl              \
	crypto           \
	irods_client     \
	stdc++           \
	boost_system     \
	boost_filesystem \
	boost_regex      \
	boost_thread     \
	boost_chrono     \
	jansson

# iRODS ships with its own version of (among others) boost. Since the version
# used by iRODS is very likely to be binary-incompatible with whatever comes
# with your Linux distro, we are required to link this specific version as
# well.
#
# See also https://github.com/irods/irods/issues/3277

LIBPATHS :=                                \
	/opt/irods-externals/boost1.60.0-0/lib \
	/opt/irods-externals/jansson2.7-0/lib

INCPATHS :=                                    \
	/opt/irods-externals/boost1.60.0-0/include \
	/opt/irods-externals/jansson2.7-0/include

WARNINGS :=                           \
	all                           \
	extra                         \
	no-unused-parameter           \
	no-missing-field-initializers \
	no-format                     \
	fatal-errors

MACROS := \
	$(addprefix DAVRODS_ENABLE_PROVIDER_, $(DAV_PROVIDERS))      \
	DAVRODS_PROVIDER_NAME=\\\"$(DAV_PROVIDER_NAME_PREFIX)\\\"    \
	DAVRODS_CONFIG_PREFIX=\\\"$(DAV_CONFIG_DIRECTIVE_PREFIX)\\\"

ifdef DEBUG
MACROS += DAVRODS_DEBUG_VERY_DESPERATE
endif

CFLAGS +=                              \
	-O2                            \
	-g3                            \
	-ggdb                          \
	-std=c99                       \
	-pedantic                      \
	$(addprefix -W, $(WARNINGS))   \
	$(addprefix -D, $(MACROS))     \
	$(addprefix -I, $(INCPATHS))

LDFLAGS +=                        \
	$(addprefix -L, $(LIBPATHS))  \
	$(addprefix -l, $(LIBS))      \
	$(addprefix -R=, $(LIBPATHS))

comma := ,

.PHONY: all shared install test clean

all: shared

install: $(SHARED)
	sudo apxs -i -n $(MODNAME)_module $(SHARED)
	sudo service httpd restart

shared: $(SHARED)

$(SHARED): $(SRCFILES)
	apxs -c                                  \
		$(addprefix -Wc$(comma), $(CFLAGS))  \
		$(addprefix -Wl$(comma), $(LDFLAGS)) \
		-o $(SHARED_FNAME) $(SRCFILES)

clean:
	rm -vf $(OUTFILES)
	rm -rvf .libs
