AUTOMAKE_OPTIONS = subdir-objects -Wno-portability

DEPDIR = .deps-util
SUBDIRS = .
SUBDIRS += grub-core

include $(top_srcdir)/conf/Makefile.common
include $(top_srcdir)/conf/Makefile.extra-dist

AM_CFLAGS = $(HOST_CFLAGS)
AM_LDFLAGS = $(HOST_LDFLAGS)
AM_CPPFLAGS = $(HOST_CPPFLAGS) $(CPPFLAGS_DEFAULT)
AM_CCASFLAGS = $(HOST_CCASFLAGS) $(CCASFLAGS_DEFAULT)

ACLOCAL_AMFLAGS = -I m4

# Install config.h into platformdir
nodist_platform_HEADERS = config.h

changelog_start_date = 2015-01-23
gitlog_to_changelog = $(top_srcdir)/build-aux/gitlog-to-changelog

ChangeLog: FORCE
	if test -d $(top_srcdir)/.git; then \
		$(gitlog_to_changelog) --srcdir=$(top_srcdir) --since=$(changelog_start_date) > '$@.tmp'; \
		rm -f '$@'; mv '$@.tmp' '$@'; \
	else \
		touch $@; \
	fi

EXTRA_DIST += ChangeLog ChangeLog-2015
