dnl Macros that test for specific features.
dnl This file is part of the Autoconf packaging for Ethereal.
dnl Copyright (C) 1998-2000 by Gerald Combs.
dnl
dnl $Id$
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
dnl 02111-1307, USA.
dnl
dnl As a special exception, the Free Software Foundation gives unlimited
dnl permission to copy, distribute and modify the configure scripts that
dnl are the output of Autoconf.  You need not follow the terms of the GNU
dnl General Public License when using or distributing such scripts, even
dnl though portions of the text of Autoconf appear in them.  The GNU
dnl General Public License (GPL) does govern all other use of the material
dnl that constitutes the Autoconf program.
dnl
dnl Certain portions of the Autoconf source text are designed to be copied
dnl (in certain cases, depending on the input) into the output of
dnl Autoconf.  We call these the "data" portions.  The rest of the Autoconf
dnl source text consists of comments plus executable code that decides which
dnl of the data portions to output in any given case.  We call these
dnl comments and executable code the "non-data" portions.  Autoconf never
dnl copies any of the non-data portions into its output.
dnl
dnl This special exception to the GPL applies to versions of Autoconf
dnl released by the Free Software Foundation.  When you make and
dnl distribute a modified version of Autoconf, you may extend this special
dnl exception to the GPL to apply to your modified version as well, *unless*
dnl your modified version has the potential to copy into its output some
dnl of the text that was the non-data portion of the version that you started
dnl with.  (In other words, unless your change moves or copies text from
dnl the non-data portions to the data portions.)  If your modification has
dnl such potential, you must delete any notice of this special exception
dnl to the GPL from your modified version.
dnl
dnl Written by David MacKenzie, with help from
dnl Franc,ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
dnl Roland McGrath, Noah Friedman, david d zuhn, and many others.
dnl Adapted for etherape by Juan Toledo (if you can call adapt to cut &
dnl paste :-) )
#
# AC_ETHEREAL_GETHOSTBY_LIB_CHECK
#
# Checks whether we need "-lnsl" to get "gethostby*()", which we use
# in "resolv.c".
#
# Adapted from stuff in the AC_PATH_XTRA macro in "acspecific.m4" in
# GNU Autoconf 2.13; the comment came from there.
# Done by Guy Harris <guy@alum.mit.edu> on 2000-01-14. 
#
AC_DEFUN([AC_ETHEREAL_GETHOSTBY_LIB_CHECK],
[
    # msh@cis.ufl.edu says -lnsl (and -lsocket) are needed for his 386/AT,
    # to get the SysV transport functions.
    # chad@anasazi.com says the Pyramid MIS-ES running DC/OSx (SVR4)
    # needs -lnsl.
    # The nsl library prevents programs from opening the X display
    # on Irix 5.2, according to dickey@clark.net.
    AC_CHECK_FUNC(gethostbyname)
    if test $ac_cv_func_gethostbyname = no; then
      AC_CHECK_LIB(nsl, gethostbyname, NSL_LIBS="-lnsl")
    fi
    AC_SUBST(NSL_LIBS)
])

#
# AC_ETHEREAL_SOCKET_LIB_CHECK
#
# Checks whether we need "-lsocket" to get "socket()", which is used
# by libpcap on some platforms - and, in effect, "gethostby*()" on
# most if not all platforms (so that it can use NIS or DNS or...
# to look up host names).
#
# Adapted from stuff in the AC_PATH_XTRA macro in "acspecific.m4" in
# GNU Autoconf 2.13; the comment came from there.
# Done by Guy Harris <guy@alum.mit.edu> on 2000-01-14. 
#
# We use "connect" because that's what AC_PATH_XTRA did.
#
AC_DEFUN([AC_ETHEREAL_SOCKET_LIB_CHECK],
[
    # lieder@skyler.mavd.honeywell.com says without -lsocket,
    # socket/setsockopt and other routines are undefined under SCO ODT
    # 2.0.  But -lsocket is broken on IRIX 5.2 (and is not necessary
    # on later versions), says simon@lia.di.epfl.ch: it contains
    # gethostby* variants that don't use the nameserver (or something).
    # -lsocket must be given before -lnsl if both are needed.
    # We assume that if connect needs -lnsl, so does gethostbyname.
    AC_CHECK_FUNC(connect)
    if test $ac_cv_func_connect = no; then
      AC_CHECK_LIB(socket, connect, SOCKET_LIBS="-lsocket",
		AC_MSG_ERROR(Function 'socket' not found.), $NSL_LIBS)
    fi
    AC_SUBST(SOCKET_LIBS)
])

#
# AC_ETHEREAL_PCAP_CHECK
#
AC_DEFUN([AC_ETHEREAL_PCAP_CHECK],
[
	# Evidently, some systems have pcap.h, etc. in */include/pcap
	AC_MSG_CHECKING(for extraneous pcap header directories)
	found_pcap_dir=""
	for pcap_dir in /usr/include/pcap /usr/local/include/pcap $prefix/include
	do
	  if test -d $pcap_dir ; then
	    CFLAGS="$CFLAGS -I$pcap_dir"
	    CPPFLAGS="$CPPFLAGS -I$pcap_dir"
	    found_pcap_dir=" $found_pcap_dir -I$pcap_dir"
	  fi
	done

	if test "$found_pcap_dir" != "" ; then
	  AC_MSG_RESULT(found --$found_pcap_dir added to CFLAGS)
	else
	  AC_MSG_RESULT(not found)
	fi

	# Pcap header checks
	AC_CHECK_HEADER(pcap.h,,
	    AC_MSG_ERROR([[Header file pcap.h not found; if you installed libpcap from source, did you also do \"make install-incl\"?]]))

	#
	# Check to see if we find "pcap_open_live" in "-lpcap".
	#
	AC_CHECK_LIB(pcap, pcap_open_live,
	  [
	    # TODO
	    # This is a dirty hack, but I don't know any other way 
	    # to do it. If you do, please tell me.
	    if test "x$STATIC_PCAP" != "xyes"; then
	      PCAP_LIBS=-lpcap
	    fi
	    if test "x$STATIC_PCAP" = "xyes"; then
	      PCAP_LIBS="-Wl,-Bstatic -lpcap -Wl,-Bdynamic"
	      echo "Forcing static pcap linking"
	    fi
	    AC_DEFINE(HAVE_LIBPCAP)
	  ], AC_MSG_ERROR(Library libpcap not found.),
	  $SOCKET_LIBS $NSL_LIBS)
	AC_SUBST(PCAP_LIBS)
])

