## Copyright (C) 2021  Trevor Woerner <twoerner@gmail.com>
## SPDX-License-Identifier: OSL-3.0

########################
## top-level Makefile.am
########################
SUBDIRS = @SUBDIRS@
DIST_SUBDIRS = @SUBDIRS@

EXTRA_DIST = LICENSE.txt README.md
ACLOCAL_AMFLAGS = "-I cfg/m4"

distcleancheck:
	$(RM) *libtool
