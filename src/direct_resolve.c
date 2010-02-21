/*
   Etherape
   Copyright (C) 2000 Juan Toledo

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
    Non-blocking DNS portion --
    Copyright (C) 1998 by Simon Kirby <sim@neato.org>
    Released under GPL, as above.

    Original code copied from mtr (www.bitwizard.nl/mtr)
    Copyright (C) 1997,1998  Matt Kimball
*/
/*
    Note: unlike the original, this code uses snprintf() to format messages, thus using much
    less memory while being safer. Some older systems unfortunately don't have snprintf().
    If you are the unfortunate owner of such system, make *all* char buffers about 1KB and
    use sprintf - at your risk!
*/


#include <config.h>
#ifdef USE_DIRECTDNS

#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if defined(__MACH__) && defined(__APPLE__)
#include <arpa/nameser_compat.h>
#else
#include <arpa/nameser.h>
#endif
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ip-cache.h"
#include "util.h"
#include "direct_resolve.h"



#ifdef NO_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(errno) (((errno) >= 0 && (errno) < sys_nerr) ? sys_errlist[errno] : "unlisted error")
#endif

/*  Hmm, it seems Irix requires this  */
extern int errno;

/* Defines */
#define HostnameLength 255	/* From RFC */
#define MaxPacketsize (PACKETSZ)

/* Typedefs */
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

#define OpcodeCount 3
char *opcodes[OpcodeCount + 1] = {
  "standard query",
  "inverse query",
  "server status request",
  "unknown",
};

#define ResponsecodeCount 6
char *responsecodes[ResponsecodeCount + 1] = {
  "no error",
  "format error in query",
  "server failure",
  "queried domain name does not exist",
  "requested query type not implemented",
  "refused by name server",
  "unknown error",
};

#define ResourcetypeCount 17
char *resourcetypes[ResourcetypeCount + 1] = {
  "unknown type",
  "A: host address",
  "NS: authoritative name server",
  "MD: mail destination (OBSOLETE)",
  "MF: mail forwarder (OBSOLETE)",
  "CNAME: name alias",
  "SOA: authority record",
  "MB: mailbox domain name (EXPERIMENTAL)",
  "MG: mail group member (EXPERIMENTAL)",
  "MR: mail rename domain name (EXPERIMENTAL)",
  "NULL: NULL RR (EXPERIMENTAL)",
  "WKS: well known service description",
  "PTR: domain name pointer",
  "HINFO: host information",
  "MINFO: mailbox or mail list information",
  "MX: mail exchange",
  "TXT: text string",
  "unknown type",
};

#define ClasstypeCount 5
char *classtypes[ClasstypeCount + 1] = {
  "unknown class",
  "IN: the Internet",
  "CS: CSNET (OBSOLETE)",
  "CH: CHAOS",
  "HS: Hesoid [Dyer 87]",
  "unknown class"
};

char *rrtypes[] = {
  "Unknown",
  "Query",
  "Answer",
  "Authority reference",
  "Resource reference",
};

enum
{
  RR_UNKNOWN,
  RR_QUERY,
  RR_ANSWER,
  RR_AUTHORITY,
  RR_RESOURCE,
};

typedef struct
{
  word id;			/* Packet id */
  byte databyte_a;
  /* rd:1           recursion desired
   * tc:1           truncated message
   * aa:1           authoritive answer
   * opcode:4       purpose of message
   * qr:1           response flag
   */
  byte databyte_b;
  /* rcode:4        response code
   * unassigned:2   unassigned bits
   * pr:1           primary server required (non standard)
   * ra:1           recursion available
   */
  word qdcount;			/* Query record count */
  word ancount;			/* Answer record count */
  word nscount;			/* Authority reference record count */
  word arcount;			/* Resource reference record count */
}
packetheader;

#ifndef HFIXEDSZ
#define HFIXEDSZ (sizeof(packetheader))
#endif

/*
 * Byte order independent macros for packetheader
 */
#define getheader_rd(x) (x->databyte_a & 1)
#define getheader_tc(x) ((x->databyte_a >> 1) & 1)
#define getheader_aa(x) ((x->databyte_a >> 2) & 1)
#define getheader_opcode(x) ((x->databyte_a >> 3) & 15)
#define getheader_qr(x) (x->databyte_a >> 7)
#define getheader_rcode(x) (x->databyte_b & 15)
#define getheader_pr(x) ((x->databyte_b >> 6) & 1)
#define getheader_ra(x) (x->databyte_b >> 7)

#if 0

/* The execution order inside an expression is undefined! That means that
   this might work, but then again, it might not... Sun Lint pointed this 
   one out... */

#define sucknetword(x) (((word)*(x) << 8) | (((x)+= 2)[-1]))
#define sucknetshort(x) (((short)*(x) << 8) | (((x)+= 2)[-1]))
#define sucknetdword(x) (((dword)*(x) << 24) | ((x)[1] << 16) | ((x)[2] << 8) | (((x)+= 4)[-1]))
#define sucknetlong(x) (((long)*(x) << 24) | ((x)[1] << 16) | ((x)[2] << 8) | (((x)+= 4)[-1]))
#else

#define sucknetword(x)  ((x)+=2,((word)  (((x)[-2] <<  8) | ((x)[-1] <<  0))))
#define sucknetshort(x) ((x)+=2,((short) (((x)[-2] <<  8) | ((x)[-1] <<  0))))
#define sucknetdword(x) ((x)+=4,((dword) (((x)[-4] << 24) | ((x)[-3] << 16) | \
                                          ((x)[-2] <<  8) | ((x)[-1] <<  0))))
#define sucknetlong(x)  ((x)+=4,((long)  (((x)[-4] << 24) | ((x)[-3] << 16) | \
                                          ((x)[-2] <<  8) | ((x)[-1] <<  0))))
#endif

static uint32_t alignedip;
static uint32_t localhost;


/*#define Debug 1*/
#ifdef Debug
static int debug = 1;
#else
static int debug = 0;
#endif

/* statistics counters */
static dword res_iplookupsuccess = 0;
static dword res_reversesuccess = 0;
static dword res_nxdomain = 0;
static dword res_nserror = 0;
static dword res_hostipmismatch = 0;
static dword res_unknownid = 0;

static int resfd = -1;			/* socket file descr. */
static char tempstring[256];		/* temporary string used as buffer ... */

/* internal funcs fwd decls */
static void direct_ready (gpointer data, gint fd, GdkInputCondition cond);
static void direct_ack (void);

/* Callback function everytime a dns_lookup function is finished */
static void
direct_ready (gpointer data, gint fd, GdkInputCondition cond)
{
  direct_ack ();
}

/* called to activate the resolver */
int 
direct_open (void)
{
  int option, i;
  res_init ();
  if (!_res.nscount)
    {
      fprintf (stderr, "No nameservers defined.\n");
      exit (-1);
    }
  _res.options |= RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  for (i = 0; i < _res.nscount; i++)
    _res.nsaddr_list[i].sin_family = AF_INET;
  resfd = socket (AF_INET, SOCK_DGRAM, 0);
  if (resfd == -1)
    {
      fprintf (stderr,
	       "Unable to allocate socket for nameserver communication: %s\n",
	       strerror (errno));
      exit (-1);
    }
  option = 1;
  if (setsockopt
      (resfd, SOL_SOCKET, SO_BROADCAST, (char *) &option, sizeof (option)))
    {
      fprintf (stderr,
	       "Unable to setsockopt() on nameserver communication socket: %s\n",
	       strerror (errno));
      exit (-1);
    }
  localhost = inet_addr ("127.0.0.1");

  /* cache activation */  
  ipcache_init();

  g_my_debug ("File descriptor for DNS is %d", resfd);
  gdk_input_add (resfd,
                 GDK_INPUT_READ, direct_ready, NULL);

  return 0;
}

/* closes the resolver */
void
direct_close(void)
{
   if (resfd == -1)
      return; /* already closed */
   close(resfd);
   resfd= -1;   
}

static void
restell (char *s)
{
  /* TODO JTC Turn this into a g_my_debug or something */
#if Debug
  fputs (s, stderr);
  fputs ("\n", stderr);
#endif
}

static void
dorequest (char *s, int type, word id)
{
  packetheader *hp;
  int r, i;
  int buf[(MaxPacketsize / sizeof (int)) + 1];

  restell ("dorequest().");
  r = res_mkquery (QUERY, s, C_IN, type, NULL, 0, NULL, (u_char *) buf,
		   MaxPacketsize);
  if (r == -1)
    {
      restell ("Resolver error: Query too large.");
      return;
    }
  hp = (packetheader *) buf;
  hp->id = id;			/* htons() deliberately left out (redundant) */
  for (i = 0; i < _res.nscount; i++)
    (void) sendto (resfd, buf, r, 0,
		   (struct sockaddr *) &_res.nsaddr_list[i],
		   sizeof (struct sockaddr));
}

/* creates a request, asking DNS for its name */
static void
sendrequest_inverse (uint32_t ip)
{
  struct ipcache_item *rp = NULL;
  rp = ipcache_prepare_request(ip);

  /* sending the request */
  snprintf (tempstring, sizeof (tempstring), "%u.%u.%u.%u.in-addr.arpa",
	    ((byte *) & rp->ip)[3],
	    ((byte *) & rp->ip)[2],
	    ((byte *) & rp->ip)[1], ((byte *) & rp->ip)[0]);
  dorequest (tempstring, T_PTR, rp->id);
  if (debug)
    {
      snprintf (tempstring, sizeof (tempstring),
		"Resolver: Sent domain lookup request for \"%s\".",
		strlongip (rp->ip));
      restell (tempstring);
    } 
}

/* handles the NOERROR response */
static void
parserespacket_noerror (byte * s, int l, struct ipcache_item *rp,
			packetheader * hp)
{
  byte *eob;
  byte *c;
  long ttl;
  int r, usefulanswer;
  word rr, datatype, class, qdatatype, qclass;
  byte rdatalength;
  char stackstring[MAXDNAME + 1];
  char namestring[MAXDNAME + 1];


  if (!hp->ancount)
    {
      restell ("Resolver error: No error returned but no answers given.");
      return;
    }

  if (debug)
    {
      snprintf (tempstring, sizeof (tempstring),
		"Resolver: Received nameserver reply. (qd:%u an:%u ns:%u ar:%u)",
		hp->qdcount, hp->ancount, hp->nscount, hp->arcount);
      restell (tempstring);
    }
  if (hp->qdcount != 1)
    {
      restell ("Resolver error: Reply does not contain one query.");
      return;
    }

  eob = s + l;
  c = s + HFIXEDSZ;

  if (c > eob)
    {
      restell ("Resolver error: Reply too short.");
      return;
    }

  if (rp->state == IPCACHE_STATE_PTRREQ)
    {
      /* Construct expected query reply */
      snprintf (stackstring, sizeof (stackstring),
		"%u.%u.%u.%u.in-addr.arpa",
		((byte *) & rp->ip)[3],
		((byte *) & rp->ip)[2],
		((byte *) & rp->ip)[1], ((byte *) & rp->ip)[0]);
    }
  else
    *stackstring = '\0';

  *namestring = '\0';
  r = dn_expand (s, s + l, c, namestring, sizeof (namestring));
  if (r == -1)
    {
      restell
	("Resolver error: dn_expand() failed while expanding query domain.");
      return;
    }
  namestring[strlen (stackstring)] = '\0';	/* truncates expanded request to len of expected request */
  if (strcasecmp (stackstring, namestring))
    {
      if (debug)
	{
	  snprintf (tempstring, sizeof (tempstring),
		    "Resolver: Unknown query packet dropped. (\"%s\" does not match \"%s\")",
		    stackstring, namestring);
	  restell (tempstring);
	}
      return;
    }
  if (debug)
    {
      snprintf (tempstring, sizeof (tempstring),
		"Resolver: Queried domain name: \"%s\"", namestring);
      restell (tempstring);
    }
  c += r;			/* skips query string */
  if (c + 4 > eob)
    {
      restell ("Resolver error: Query resource record truncated.");
      return;
    }
  qdatatype = sucknetword (c);
  qclass = sucknetword (c);
  if (qclass != C_IN)
    {
      snprintf (tempstring, sizeof (tempstring),
		"Resolver error: Received unsupported query class: %u (%s)",
		qclass,
		qclass <
		ClasstypeCount ? classtypes[qclass] :
		classtypes[ClasstypeCount]);
      restell (tempstring);
    }
  switch (qdatatype)
    {
    case T_PTR:
      if (rp->state != IPCACHE_STATE_PTRREQ)
	if (debug)
	  {
	    restell
	      ("Resolver warning: Ignoring response with unexpected query type \"PTR\".");
	    return;
	  }
      break;
    default:
      snprintf (tempstring, sizeof (tempstring),
		"Resolver error: Received unimplemented query type: %u (%s)",
		qdatatype,
		qdatatype < ResourcetypeCount ?
		resourcetypes[qdatatype] : resourcetypes[ResourcetypeCount]);
      restell (tempstring);
    }
  for (rr = hp->ancount + hp->nscount + hp->arcount; rr; rr--)
    {
      if (c > eob)
	{
	  restell
	    ("Resolver error: Packet does not contain all specified resouce records.");
	  return;
	}
      *namestring = '\0';
      r = dn_expand (s, s + l, c, namestring, sizeof (namestring));
      if (r == -1)
	{
	  restell
	    ("Resolver error: dn_expand() failed while expanding answer domain.");
	  return;
	}
      namestring[strlen (stackstring)] = '\0';
      if (strcasecmp (stackstring, namestring))
	usefulanswer = 0;
      else
	usefulanswer = 1;
      if (debug)
	{
	  snprintf (tempstring, sizeof (tempstring),
		    "Resolver: answered domain query: \"%s\"", namestring);
	  restell (tempstring);
	}
      c += r;
      if (c + 10 > eob)
	{
	  restell ("Resolver error: Resource record truncated.");
	  return;
	}
      datatype = sucknetword (c);
      class = sucknetword (c);
      ttl = sucknetlong (c);
      rdatalength = sucknetword (c);
      if (class != qclass)
	{
	  snprintf (tempstring, sizeof (tempstring),
		    "query class: %u (%s)",
		    qclass,
		    qclass < ClasstypeCount ?
		    classtypes[qclass] : classtypes[ClasstypeCount]);
	  restell (tempstring);
	  snprintf (tempstring, sizeof (tempstring),
		    "rr class: %u (%s)", class,
		    class < ClasstypeCount ?
		    classtypes[class] : classtypes[ClasstypeCount]);
	  restell (tempstring);
	  restell
	    ("Resolver error: Answered class does not match queried class.");
	  return;
	}
      if (!rdatalength)
	{
	  restell ("Resolver error: Zero size rdata.");
	  return;
	}
      if (c + rdatalength > eob)
	{
	  restell
	    ("Resolver error: Specified rdata length exceeds packet size.");
	  return;
	}
      if (datatype == qdatatype || datatype == T_CNAME)
	{
	  if (debug)
	    {
	      char sendstring[128];
	      snprintf (tempstring, sizeof (tempstring), "Resolver: TTL: %s",
			strtdiff (sendstring, sizeof (sendstring), ttl));
	      restell (tempstring);
	    }
	  if (usefulanswer)
	    switch (datatype)
	      {
	      case T_A:
		if (rdatalength != 4)
		  { 
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver error: Unsupported rdata format for \"A\" type. (%u bytes)",
			      rdatalength);
		    restell (tempstring);
		    return;
		  }
		if (memcmp (&rp->ip, (uint32_t *) c, sizeof (uint32_t)))
		  {
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver: Reverse authentication failed: %s != ",
			      strlongip (rp->ip));
		    memcpy (&alignedip, (uint32_t *) c, sizeof (uint32_t));
		    safe_strncat (tempstring, strlongip (alignedip),
				  sizeof (tempstring));
		    restell (tempstring);
		    res_hostipmismatch++;
		    ipcache_request_failed (rp);
		  }
		else
		  {
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver: Reverse authentication complete: %s == \"%s\".",
			      strlongip (rp->ip),
			      (rp->fq_hostname) ? rp->fq_hostname : "");
		    restell (tempstring);
		    res_reversesuccess++;
		    ipcache_request_succeeded (rp, ttl, NULL);
		    return; 
		  }
		break;
	      case T_PTR:
	      case T_CNAME:
		*namestring = '\0';
		r = dn_expand (s, s + l, c, namestring, sizeof (namestring));
		if (r == -1)
		  {
		    restell
		      ("Resolver error: dn_expand() failed while expanding domain in rdata.");
		    return;
		  }
		if (debug)
		  {
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver: Answered domain: \"%s\"",
			      namestring);
		    restell (tempstring);
		  }
		if (r > HostnameLength)
		  {
		    restell ("Resolver error: Domain name too long.");
		    ipcache_request_failed (rp);
		    return;
		  }
		if (datatype == T_CNAME)
		  {
		    safe_strncpy (stackstring, namestring,
				  sizeof (stackstring));
		    break;
		  }
                ipcache_request_succeeded (rp, ttl, namestring);
                res_iplookupsuccess++;
		break;
	      default:
		snprintf (tempstring, sizeof (tempstring),
			  "Resolver error: Received unimplemented data type: %u (%s)",
			  datatype,
			  datatype <
			  ResourcetypeCount
			  ?
			  resourcetypes
			  [datatype] : resourcetypes[ResourcetypeCount]);
		restell (tempstring);
	      }
	}
      else
	{
	  if (debug)
	    {
	      snprintf (tempstring, sizeof (tempstring),
			"Resolver: Ignoring resource type %u. (%s)",
			datatype,
			datatype <
			ResourcetypeCount ?
			resourcetypes
			[datatype] : resourcetypes[ResourcetypeCount]);
	      restell (tempstring);
	    }
	}
      c += rdatalength;
    }
}

static void
direct_parserespacket (byte * s, int l)
{
  struct ipcache_item *rp;
  packetheader *hp;

  if (l < sizeof (packetheader))
    {
      restell ("Resolver error: Packet smaller than standard header size.");
      return;
    }
  if (l == sizeof (packetheader))
    {
      restell ("Resolver error: Packet has empty body.");
      return;
    }
  hp = (packetheader *) s;
  /* Convert data to host byte order */
  /* hp->id does not need to be redundantly byte-order flipped, it is only echoed by nameserver */
  rp = ipcache_findid (hp->id);
  if (!rp)
    {
      res_unknownid++;
      return;
    }
  if ((rp->state == IPCACHE_STATE_FINISHED) || (rp->state == IPCACHE_STATE_FAILED))
    return;
  hp->qdcount = ntohs (hp->qdcount);
  hp->ancount = ntohs (hp->ancount);
  hp->nscount = ntohs (hp->nscount);
  hp->arcount = ntohs (hp->arcount);
  if (getheader_tc (hp))
    {				/* Packet truncated */
      restell ("Resolver error: Nameserver packet truncated.");
      return;
    }
  if (!getheader_qr (hp))
    {				/* Not a reply */
      restell
	("Resolver error: Query packet received on nameserver communication socket.");
      return;
    }
  if (getheader_opcode (hp))
    {				/* Not opcode 0 (standard query) */
      restell ("Resolver error: Invalid opcode in response packet.");
      return;
    }
  switch (getheader_rcode (hp))
    {
    case NOERROR:
      parserespacket_noerror (s, l, rp, hp);
      break;
    case NXDOMAIN:
      if (debug)
	restell ("Resolver: Host not found.");
      res_nxdomain++;
      ipcache_request_failed (rp);
      break;
    default:
      snprintf (tempstring, sizeof (tempstring),
		"Resolver: Received error response %u. (%s)",
		getheader_rcode (hp),
		getheader_rcode (hp) <
		ResponsecodeCount ?
		responsecodes[getheader_rcode (hp)] :
		responsecodes[ResponsecodeCount]);
      restell (tempstring);
      res_nserror++;
    }
}

void
direct_ack ()
{
  /* rcv buffer */
  dword resrecvbuf[(MaxPacketsize + 7) >> 2];	/* MUST BE DWORD ALIGNED */
  struct sockaddr_in from;	/* sending addr buffer */
  int fromlen = sizeof (struct sockaddr_in);	/* s.addr buflen */
  int r, i;

  /* packet rcvd, read it */
  r = recvfrom (resfd, (byte *) resrecvbuf, MaxPacketsize, 0,
		(struct sockaddr *) &from, &fromlen);
  if (r > 0)
    {
      /* Check to see if this server is actually one we sent to */
      if (from.sin_addr.s_addr == localhost)
	{
	  for (i = 0; i < _res.nscount; i++)
	    if ((_res.nsaddr_list[i].sin_addr.s_addr == from.sin_addr.s_addr) || (!_res.nsaddr_list[i].sin_addr.s_addr))	/* 0.0.0.0 replies as 127.0.0.1 */
	      break;
	}
      else
	for (i = 0; i < _res.nscount; i++)
	  if (_res.nsaddr_list[i].sin_addr.s_addr == from.sin_addr.s_addr)
	    break;
      if (i == _res.nscount)
	{
	  snprintf (tempstring, sizeof (tempstring),
		    "Resolver error: Received reply from unknown source: %s",
		    strlongip (from.sin_addr.s_addr));
	  restell (tempstring);
	}
      else
	direct_parserespacket ((byte *) resrecvbuf, r);
    }
  else
    {
      snprintf (tempstring, sizeof (tempstring), "Resolver: Socket error: %s",
		strerror (errno));
      restell (tempstring);
    }
}

const char *
direct_lookup (uint32_t ip)
{
  const char *ipname;
  int is_expired = 0;
  
  /* asks cache */
  ipname = ipcache_getnameip(ip, &is_expired);

  if (is_expired)
    {
      sendrequest_inverse (ip); /* request needed */ 
      if (debug)
        {
         snprintf (tempstring, sizeof (tempstring),
                   "Resolver: resend : %s \n", strlongip (ip));
         restell (tempstring);
        }
    }
    
  if (debug)
    {
      snprintf (tempstring, sizeof (tempstring),
                "Resolver: Name decoded: %s == \"%s\".\n",
                strlongip (ip), ipname);
      restell (tempstring);
    }
    
  return ipname;
}

#endif /* USE_DIRECTDNS*/
