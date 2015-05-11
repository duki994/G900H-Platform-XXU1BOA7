

INCLUDES = -I$(top_srcdir)/src/libstrongswan -I$(top_srcdir)/src/libhydra \
	-I$(top_srcdir)/src/libcharon

AM_CFLAGS = -rdynamic

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-android.la
else
plugin_LTLIBRARIES = libstrongswan-android.la
endif

libstrongswan_android_la_SOURCES = \
	android_plugin.c android_plugin.h \
	android_service.c android_service.h \
	android_handler.c android_handler.h \
	android_creds.c android_creds.h \
	android_shared_key_credential.c android_shared_key_credential.h \
	android_ikev1_utils.c android_ikev1_utils.h \
	android_ikev2_utils.c android_ikev2_utils.h

libstrongswan_android_la_LDFLAGS = -module -avoid-version
libstrongswan_android_la_LIBADD  = -lcutils
