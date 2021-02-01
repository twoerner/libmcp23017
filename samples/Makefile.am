## Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
## SPDX-License-Identifier: OSL-3.0

########################
## samples/Makefile.am
########################
SUBDIRS =

AM_CFLAGS = -Wall -Werror -Wextra -Wconversion -Wreturn-type -Wstrict-prototypes \
	-I$(top_srcdir)/lib

noinst_PROGRAMS = mcp23017 mcp23017util
mcp23017util_LDADD = $(top_builddir)/lib/libmcp23017.la
