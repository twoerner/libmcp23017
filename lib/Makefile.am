## Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
## SPDX-License-Identifier: OSL-3.0

########################
## lib/Makefile.am
########################
SUBDIRS =
AM_CFLAGS = -Wall -Werror -Wextra -Wconversion -Wreturn-type -Wstrict-prototypes
pkginclude_HEADERS = mcp23017.h

########################
## shared lib
########################
lib_LTLIBRARIES = libmcp23017.la
libmcp23017_la_SOURCES = mcp23017.c mcp23017.h
libmcp23017_la_LDFLAGS =  -release @VERSION@
libmcp23017_la_LDFLAGS += -version-info 1:0:1
## C:R:A
## any code change       -> inc(R)
## interface add/del/chg -> R=0, inc(C)
## interface added       -> inc(A)
## interfece removed     -> A=0
