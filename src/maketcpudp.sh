#!/bin/sh

# Etherape
# Copyright (C) 2000 Juan Toledo
# \$Id$
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

export O_FILE=tcpudp.h

cat > $O_FILE <<-EOF
	/* This file is automatically created by make tcp.h */
	
	/* Etherape
	* Copyright (C) 2000 Juan Toledo
	* \$Id$
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License
	* along with this program; if not, write to the Free Software
	* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	*/
EOF

# We create a temporary file with services names and port numbers
grep "/tcp" /etc/services |
tr [a-z] [A-Z]  | \
tr - _ | \
sed -e "/^#/ d" | \
sed -e "s/\([^ 	]*\).*/\1/" > /tmp/tcp.h

COUNT=`cat /tmp/tcp.h | wc -l`

echo "#define TCP_SERVICES $COUNT" >> $O_FILE
echo "typedef enum {" >> $O_FILE

# We construct the enums for the port numbers

grep "/tcp" /etc/services | \
tr [a-z] [A-Z]  | \
tr - _ | \
sed -e "/^#/ d" | \
sed -e "s/\(.*\)\/TCP[^#]*\(.*\)/\1,	\2/" | \
sed -e "s/\(.*\)\#\(.*\)/\1\/\*\2 \*\//" | \
sed -e "s/\([^ 	]*\)[ 	]*\(.*\)/\1=\2/" | \
sed -e "s/\(.*\)/TCP_\1/" | \
 cat \
>> $O_FILE
echo "TCP_DUMMY=99999" >> $O_FILE
echo "}	tcp_type_t;" >> $O_FILE

# Now the same with UDP

# We create a temporary file with services names and port numbers
grep "/udp" /etc/services |
tr [a-z] [A-Z]  | \
tr - _ | \
sed -e "/^#/ d" | \
sed -e "s/\([^ 	]*\).*/\1/" > /tmp/udp.h

COUNT=`cat /tmp/udp.h | wc -l`

echo "#define UDP_SERVICES $COUNT" >> $O_FILE
echo "typedef enum {" >> $O_FILE

# We construct the enums for the port numbers

grep "/udp" /etc/services | \
tr [a-z] [A-Z]  | \
tr - _ | \
sed -e "/^#/ d" | \
sed -e "s/\(.*\)\/UDP[^#]*\(.*\)/\1,	\2/" | \
sed -e "s/\(.*\)\#\(.*\)/\1\/\*\2 \*\//" | \
sed -e "s/\([^ 	]*\)[ 	]*\(.*\)/\1=\2/" | \
sed -e "s/\(.*\)/UDP_\1/" | \
 cat \
>> $O_FILE
echo "UDP_DUMMY=99999" >> $O_FILE
echo "}	udp_type_t;" >> $O_FILE

cat >> $O_FILE <<-EOF

	typedef struct
	{
	  tcp_type_t number;
	  gchar *name;
	} tcp_service_t;
	
	static tcp_service_t tcp_services_table [TCP_SERVICES+1] = {
EOF

cat /tmp/tcp.h | \
sed -e "s/\(.*\)/\{TCP_\1, \"\1\"\}, /" \
>> $O_FILE
# dummy entry
echo "{TCP_DUMMY, \"DUMMY\"} };" >> $O_FILE


cat >> $O_FILE <<-EOF

	typedef struct
	{
	  udp_type_t number;
	  gchar *name;
	} udp_service_t;
	
	static udp_service_t udp_services_table [UDP_SERVICES+1] = {
EOF

cat /tmp/udp.h | \
sed -e "s/\(.*\)/\{UDP_\1, \"\1\"\}, /" \
>> $O_FILE
# dummy entry
echo "{UDP_DUMMY, \"DUMMY\"} };" >> $O_FILE


indent $O_FILE
