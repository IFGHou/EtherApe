## Process this file with automake to produce Makefile.in

# $Id$


confdir = $(sysconfdir)/etherape
gladedir   = $(datadir)/etherape/glade

INCLUDES = \
	-I$(top_srcdir)/intl \
	-DGLADEDIR=\""$(gladedir)"\" \
	-DCONFDIR=\""$(confdir)"\" \
	$(ETHERAPE_CFLAGS) 

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
	info_windows.c info_windows.h \
	conversations.c conversations.h \
	gnomesupport.h

etherape_LDADD = $(ETHERAPE_LIBS) $(PCAP_LIBS) 
etherape_LDFLAGS = 
etherape_LINK = $(LIBTOOL) --mode=link $(CC) $(CFLAGS) $(LDFLAGS) -o $@

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak
