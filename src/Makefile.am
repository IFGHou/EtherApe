## Process this file with automake to produce Makefile.in

# $Id$


confdir = $(sysconfdir)/etherape
gladedir   = $(datadir)/etherape/glade

INCLUDES = \
	-I$(top_srcdir)/intl \
	-DGLADEDIR=\""$(gladedir)"\" \
	-DCONFDIR=\""$(confdir)"\" \
	$(GNOME_INCLUDEDIR)

bin_PROGRAMS = etherape

etherape_SOURCES = \
	resolv.c eth_resolv.h \
	util.c util.h \
	dns.c dns.h \
	callbacks.c callbacks.h \
	menus.c menus.h \
	preferences.c preferences.h \
	main.c main.h globals.h\
	diagram.c diagram.h \
	capture.c capture.h \
	names.c names.h \
	names_netbios.c names_netbios.h \
	protocols.c protocols.h \
	prot_types.h \
	gnomesupport.h

etherape_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) $(PCAP_LIBS)

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak
