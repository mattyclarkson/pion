# --------------------------------
# pion automake configuration file
# --------------------------------

AM_CPPFLAGS = -I../include

lib_LTLIBRARIES = libpion.la

libpion_la_SOURCES = \
	admin_rights.cpp algorithm.cpp logger.cpp plugin.cpp process.cpp scheduler.cpp \
	spdy_decompressor.cpp spdy_parser.cpp \
	tcp_server.cpp tcp_timer.cpp \
	http_auth.cpp http_basic_auth.cpp http_cookie_auth.cpp http_message.cpp \
	http_parser.cpp http_plugin_server.cpp http_reader.cpp http_server.cpp \
	http_types.cpp http_writer.cpp

PION_MAJOR_VERSION = $(shell echo $(PACKAGE_VERSION) | sed 's/\([0-9]\+\).\([0-9]\+\).\([0-9]\+\)/\1/')
PION_MINOR_VERSION = $(shell echo $(PACKAGE_VERSION) | sed 's/\([0-9]\+\).\([0-9]\+\).\([0-9]\+\)/\2/')
PION_PATCH_VERSION = $(shell echo $(PACKAGE_VERSION) | sed 's/\([0-9]\+\).\([0-9]\+\).\([0-9]\+\)/\3/')
PION_CURRENT_VERSION = $(shell expr $(PION_MAJOR_VERSION) \* 100 + $(PION_MINOR_VERSION))
PION_REVISION_VERSION = $(PION_PATCH_VERSION)
PION_AGE_VERSION = $(PION_MINOR_VERSION)
PION_VERSION_INFO = $(PION_CURRENT_VERSION):$(PION_REVISION_VERSION):$(PION_AGE_VERSION)
libpion_la_LDFLAGS = -no-undefined -version-info $(PION_VERSION_INFO)
libpion_la_LIBADD = @PION_EXTERNAL_LIBS@

EXTRA_DIST = *.vcxproj *.vcxproj.filters
