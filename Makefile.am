## Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
## SPDX-License-Identifier: OSL-3.0

########################
## top-level Makefile.am
########################
SUBDIRS = @SUBDIRS@
DIST_SUBDIRS = @SUBDIRS@

EXTRA_DIST = LICENSE.txt README.md libmcp23017.pc.in
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmcp23017.pc

distcleancheck:
	$(RM) *libtool
