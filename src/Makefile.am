## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = etherape

etherape_SOURCES = \
	main.c \
	capture.c capture.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h \
	gnomesupport.h

etherape_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS)

