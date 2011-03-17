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
	export.c export.h \
	dns.c dns.h \
	direct_resolve.c direct_resolve.h \
	thread_resolve.c thread_resolve.h \
	callbacks.c callbacks.h \
	menus.c menus.h \
	preferences.c preferences.h \
	pref_dialog.c pref_dialog.h \
	main.c main.h \
	appdata.c appdata.h\
	diagram.c diagram.h \
	capture.c capture.h \
	names.c names.h \
	names_netbios.c names_netbios.h \
	protocols.c protocols.h \
	decode_proto.c decode_proto.h \
	prot_types.h pkt_info.h \
	info_windows.c info_windows.h \
	ip-cache.c ip-cache.h \
	node_id.c node_id.h \
	node.c node.h \
	node_windows.c node_windows.h \
	links.c links.h \
	conversations.c conversations.h \
	basic_stats.c basic_stats.h \
	traffic_stats.c traffic_stats.h \
	datastructs.c datastructs.h \
	ui_utils.c ui_utils.h

etherape_LDADD = $(ETHERAPE_LIBS) $(PCAP_LIBS) 
etherape_LDFLAGS = 

AM_CPPFLAGS = \
	$(ETHERAPE_CFLAGS)

.indent: $(etherape_SOURCES)
	indent $? 
	touch .indent
indent: .indent 

purge:
	rm -f *~ *.bak

splint:
	splint -I.. +posixlib +gnuextensions +charintliteral -nullassign $(INCLUDES) $(etherape_SOURCES)
