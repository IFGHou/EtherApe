## Process this file with automake to produce Makefile.in

INCLUDES = \
	@GTK_CFLAGS@

bin_PROGRAMS = etherape

etherape_SOURCES = \
	main.c \
	capture.c capture.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h 

etherape_LDADD = @GTK_LIBS@

