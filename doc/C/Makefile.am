## Process this file with automake to produce Makefile.in.

index.html : etherape.sgml
	db2html etherape.sgml
	mv etherape/*.html .
	rm -rf etherape
	

EXTRA_DIST = etherape.sgml \
             index.html  \
             authors.html \
             license.html \
             usage.html \
             bugs.html \
	     ln7.html \
	     topic.dat
#	     etherape-main.png 

	     

etherape_helpdir = $(datadir)/gnome/help/etherape/C
etherape_help_DATA = index.html \
                  authors.html \
		  license.html \
                  usage.html \
		  bugs.html \
		  ln7.html \
		  topic.dat
#		  etherape-main.png 
