## Process this file with automake to produce Makefile.in

# $Id$


confdir = $(sysconfdir)/etherape
gladedir   = $(datadir)/etherape/glade

INCLUDES = \
	-I$(top_srcdir)/intl \
	-DGLADEDIR=\""$(gladedir)"\" \
	-DCONFDIR=\""$(confdir)"\" \
	$(GNOME_INCLUDEDIR)

sbin_PROGRAMS = etherape

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
	protocols.c protocols.h \
	prot_types.h \
	gnomesupport.h

etherape_LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(INTLLIBS) $(PCAP_LIBS)

install-exec-local:
	ln -f $(sbindir)/etherape $(sbindir)/interape

uninstall-local:
	rm -f $(sbindir)/interape

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak
