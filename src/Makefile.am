## Process this file with automake to produce Makefile.in

# $Id$


confdir = $(sysconfdir)/etherape
gladedir   = $(datadir)/etherape/glade

INCLUDES = \
	-I$(top_srcdir)/intl \
	-DGLADEDIR=\""$(gladedir)"\" \
	-DCONFDIR=\""$(confdir)"\" \
	$(WARN_CFLAGS) \
	$(ETHERAPE_CFLAGS) 

bin_PROGRAMS = etherape

etherape_SOURCES = \
	resolv.c eth_resolv.h \
	util.c util.h \
	dns.c dns.h \
	direct_resolve.c direct_resolve.h \
	thread_resolve.c thread_resolve.h \
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
	ip-cache.c ip-cache.h \
	conversations.c conversations.h \
	datastructs.c datastructs.h

etherape_LDADD = $(ETHERAPE_LIBS) $(PCAP_LIBS) 
etherape_LDFLAGS = -Wl,--export-dynamic
etherape_LINK = $(LIBTOOL) --mode=link $(CC) $(CFLAGS) $(LDFLAGS) -o $@

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak

splint:
	splint $(INCLUDES) $(etherape_SOURCES)
