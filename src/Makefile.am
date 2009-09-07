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
	common.h \
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
	decode_proto.c decode_proto.h \
	prot_types.h pkt_info.h \
	info_windows.c info_windows.h \
	ip-cache.c ip-cache.h \
	node.c node.h \
	links.c links.h \
	conversations.c conversations.h \
	traffic_stats.c traffic_stats.h \
	datastructs.c datastructs.h

etherape_LDADD = $(ETHERAPE_LIBS) $(PCAP_LIBS) 
etherape_LDFLAGS = 

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak

splint:
	splint -I.. +posixlib +gnuextensions $(INCLUDES) $(etherape_SOURCES)
