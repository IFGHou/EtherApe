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
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef NO_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(errno) (((errno) >= 0 && (errno) < sys_nerr) ? sys_errlist[errno] : "unlisted error")
#endif



/*  Hmm, it seems Irix requires this  */
extern int errno;

/* Defined in main.c - non zero if you want to activate DNS resolving, zero to disable*/
extern int dns;

/* Defines */

#define BashSize 8192		/* Size of hash tables */
#define BashModulo(x) ((x) & 8191)	/* Modulo for hash table size: */
#define HostnameLength 255	/* From RFC */

/* Typedefs */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef unsigned int ip_t;

/* Structures */

struct resolve
{
  /* these linked lists are used for hash table overflow */
  struct resolve *nextid;
  struct resolve *previousid;
  struct resolve *nextip;
  struct resolve *previousip;

  /* linked list of struct for purging */
  struct resolve *next_active;
  struct resolve *previous_active;

  unsigned long expire_tick;	/* when current_tick > expire_tick this node is expired */
  unsigned long last_expire_tick;	/* used to escalate old timers ... */
  char *fq_hostname;		/* fully qualified hostname */
  char *only_hostname;		/* hostname without domain */
  ip_t ip;			/* ip addr */
  word id;			/* unique item id */
  byte state;			/* current state */
};

/* Non-blocking nameserver interface routines */

#define MaxPacketsize (PACKETSZ)

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

enum
{
  STATE_FINISHED,		/* query successful */
  STATE_FAILED,			/* query failed */
  STATE_PTRREQ			/* PTR query packet sent */
};

#define Is_PTR(x) (x->state == STATE_PTRREQ)

struct resolve *idbash[BashSize];
struct resolve *ipbash[BashSize];

/* max cache handling ... */
static struct resolve *active_list = NULL;	/* list of active item for purging */
static long num_active = 0;
static long max_active = 1024;

ip_t alignedip;
ip_t localhost;

#ifdef Debug
int debug = 1;
#else
int debug = 0;
#endif

/* statistics counters */
dword res_iplookupsuccess = 0;
dword res_reversesuccess = 0;
dword res_nxdomain = 0;
dword res_nserror = 0;
dword res_hostipmismatch = 0;
dword res_unknownid = 0;
dword res_resend = 0;
dword res_timeout = 0;
dword resolvecount = 0;

long idseed = 0xdeadbeef;
long aseed;

int resfd;			/* socket file descr. */

char tempstring[256];		/* temporary string used as buffer ... */

/* Code */


/* internal funcs fwd decls */
void unlinkresolve (struct resolve *rp);
char *dns_lookup2 (ip_t address, int fqdn);
void dns_events (double *sinterval);
char *strlongip (ip_t address);
void failrp (struct resolve *rp);

/* current tick */
static unsigned long current_tick = 0;

/* called to signal a new tick
   Note:
   No provisions made to prevent counter wrapping. With a 32-bit counter and a tick 
   of 10secs, the counter will wrap every 1300 years or so of continuous running.
*/
unsigned int
dns_tick ()
{
  ++current_tick;
  return 1;			/* returning true means "keep the timer running" */
}

/* return the current tick plus the delay specified and a small pseudorandom offset
   to prevent a "dns storm" when many addresses are requested concurrently 
   Note:
   Delays over 86400 ticks (10 days on the std tick len of 10secs) will be
   forced to 86400
*/
void
calc_expire_tick (struct resolve *rp, unsigned long delay, int is_request)
{
  if (delay > 86400)
    delay = 86400;
  if (is_request)
    rp->expire_tick = delay + current_tick + rand () % 12;	/* request - random delay limited to 2 minutes */
  else
    rp->expire_tick = delay + current_tick + rand () % 720;	/* expire - random delay: 2 hours */
  rp->last_expire_tick = rp->expire_tick;
}

/* returns true if tick expired */
int
is_expired_tick (unsigned long tick)
{
  return (tick < current_tick);
}

/* safe strncpy */
char *
safe_strncpy (char *dst, const char *src, size_t maxlen)
{
  if (maxlen < 1)
    return dst;
  strncpy (dst, src, maxlen - 1);	/* no need to copy that last char */
  dst[maxlen - 1] = '\0';
  return dst;
}

/* safe strncat */
char *
safe_strncat (char *dst, const char *src, size_t maxlen)
{
  size_t lendst = strlen (dst);
  if (lendst >= maxlen)
    return dst;			/* already full, nothing to do */
  strncat (dst, src, maxlen - strlen (dst));
  dst[maxlen - 1] = '\0';
  return dst;
}

/* converts a long containing a time value expressed in second to a (passed) string buffer and returns it */
char *
strtdiff (char *d, size_t lend, long signeddiff)
{
  dword diff;
  dword seconds, minutes, hours;
  long days;
  if ((diff = labs (signeddiff)))
    {
      seconds = diff % 60;
      diff /= 60;
      minutes = diff % 60;
      diff /= 60;
      hours = diff % 24;
      days = signeddiff / (60 * 60 * 24);
      if (days)
	snprintf (d, lend, "%lid", days);
      else
	*d = '\0';
      if (hours)
	snprintf (d + strlen (d), lend - strlen (d), "%luh", hours);
      if (minutes)
	snprintf (d + strlen (d), lend - strlen (d), "%lum", minutes);
      if (seconds)
	snprintf (d + strlen (d), lend - strlen (d), "%lus", seconds);
    }
  else
    snprintf (d, lend, "0s");
  return d;
}

int
issetfd (fd_set * set, int fd)
{
  return (int) ((FD_ISSET (fd, set)) && 1);
}

void
setfd (fd_set * set, int fd)
{
  FD_SET (fd, set);
}

void
clearfd (fd_set * set, int fd)
{
  FD_CLR (fd, set);
}

void
clearset (fd_set * set)
{
  FD_ZERO (set);
}

char *
strlongip (ip_t ip)
{
  struct in_addr a;
  a.s_addr = htonl (ip);
  return inet_ntoa (a);
}

ip_t
longipstr (char *s)
{
  return inet_addr (s);
}

int
dns_waitfd ()
{
  return resfd;
}

/* called to activate the resolver */
void
dns_open ()
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
  localhost = longipstr ("127.0.0.1");
  aseed = time (NULL) ^ (time (NULL) << 3) ^ (dword) getpid ();
  for (i = 0; i < BashSize; i++)
    {
      idbash[i] = NULL;
    }
}

struct resolve *
allocresolve ()
{
  struct resolve *rp;
  rp = (struct resolve *) malloc (sizeof (struct resolve));
  if (!rp)
    {
      fprintf (stderr, "malloc() failed: %s\n", strerror (errno));
      exit (-1);
    }
  memset (rp, 0, sizeof (struct resolve));
  return rp;
}

dword
getidbash (word id)
{
  return (dword) BashModulo (id);
}

dword
getipbash (ip_t ip)
{
  return (dword) BashModulo (ip);
}

/* removes rp from the active list */
static void
unlink_activelist (struct resolve *rp)
{
  /* unlink only if linked */
  if (rp->next_active)
    {
      if (rp->next_active == rp)
	{
	  /* last item of list */
	  active_list = NULL;
	  num_active = 0;
	}
      else
	{
	  rp->next_active->previous_active = rp->previous_active;
	  rp->previous_active->next_active = rp->next_active;
	  if (active_list == rp)
	    active_list = rp->next_active;
	  num_active--;
	}
    }
}

/* makes room on active list if needed */
static void
purge_activelist (void)
{
  if (num_active > max_active && num_active > 10)
    {
      /* too much items, purge oldest items until we have some room */
      while (active_list && num_active > 0 && num_active >= max_active - 10)
	{
	  struct resolve *rp = active_list->previous_active;
	  unlinkresolve (rp);
	  free (rp);
	}
    }
}

/* adds or moves rp to head of the active list */
static void
link_activelist (struct resolve *rp)
{
#ifdef Debug
  fprintf (stderr, "  active list size: %ld\n", num_active);
#endif

  /* skip if rp is already the head */
  if (active_list != rp)
    {
      if (NULL == active_list)
	{
	  /* list empty */
	  active_list = rp->next_active = rp->previous_active = rp;
	  num_active = 1;
	}
      else
	{
	  if (NULL == rp->next_active)
	    purge_activelist ();	/* new element, make sure there is room ... */
	  else
	    unlink_activelist (rp);	/* moving element, first unlink, then relink */

	  /* link at head */
	  rp->next_active = active_list;
	  rp->previous_active = active_list->previous_active;

	  active_list->previous_active->next_active = rp;
	  active_list->previous_active = rp;

	  active_list = rp;
	  num_active++;
	}
    }
}

void
linkresolveid (struct resolve *addrp)
{
  struct resolve *rp;
  dword bashnum;
  bashnum = getidbash (addrp->id);
  rp = idbash[bashnum];
  if (rp)
    {
      while ((rp->nextid) && (addrp->id > rp->nextid->id))
	rp = rp->nextid;
      while ((rp->previousid) && (addrp->id < rp->previousid->id))
	rp = rp->previousid;
      if (rp->id < addrp->id)
	{
	  addrp->previousid = rp;
	  addrp->nextid = rp->nextid;
	  if (rp->nextid)
	    rp->nextid->previousid = addrp;
	  rp->nextid = addrp;
	}
      else
	{
	  addrp->previousid = rp->previousid;
	  addrp->nextid = rp;
	  if (rp->previousid)
	    rp->previousid->nextid = addrp;
	  rp->previousid = addrp;
	}
    }
  else
    addrp->nextid = addrp->previousid = NULL;
  idbash[bashnum] = addrp;
}

void
unlinkresolveid (struct resolve *rp)
{
  dword bashnum;
  bashnum = getidbash (rp->id);
  if (idbash[bashnum] == rp)
    {
      if (rp->previousid)
	idbash[bashnum] = rp->previousid;
      else
	idbash[bashnum] = rp->nextid;
    }
  if (rp->nextid)
    rp->nextid->previousid = rp->previousid;
  if (rp->previousid)
    rp->previousid->nextid = rp->nextid;
}

void
linkresolveip (struct resolve *addrp)
{
  struct resolve *rp;
  dword bashnum;
  bashnum = getipbash (addrp->ip);
  rp = ipbash[bashnum];
  if (rp)
    {
      while ((rp->nextip) && (addrp->ip > rp->nextip->ip))
	rp = rp->nextip;
      while ((rp->previousip) && (addrp->ip < rp->previousip->ip))
	rp = rp->previousip;
      if (rp->ip < addrp->ip)
	{
	  addrp->previousip = rp;
	  addrp->nextip = rp->nextip;
	  if (rp->nextip)
	    rp->nextip->previousip = addrp;
	  rp->nextip = addrp;
	}
      else
	{
	  addrp->previousip = rp->previousip;
	  addrp->nextip = rp;
	  if (rp->previousip)
	    rp->previousip->nextip = addrp;
	  rp->previousip = addrp;
	}
    }
  else
    addrp->nextip = addrp->previousip = NULL;
  ipbash[bashnum] = addrp;
}

void
unlinkresolveip (struct resolve *rp)
{
  dword bashnum;
  bashnum = getipbash (rp->ip);
  if (ipbash[bashnum] == rp)
    {
      if (rp->previousip)
	ipbash[bashnum] = rp->previousip;
      else
	ipbash[bashnum] = rp->nextip;
    }
  if (rp->nextip)
    rp->nextip->previousip = rp->previousip;
  if (rp->previousip)
    rp->previousip->nextip = rp->nextip;
}


void
unlinkresolve (struct resolve *rp)
{
  unlink_activelist (rp);	/* removes from purge list */
  unlinkresolveid (rp);
  unlinkresolveip (rp);

  if (rp->fq_hostname)
    free (rp->fq_hostname);
  if (rp->only_hostname)
    free (rp->only_hostname);
}

struct resolve *
findid (word id)
{
  struct resolve *rp;
  int bashnum;
  bashnum = getidbash (id);
  rp = idbash[bashnum];
  if (rp)
    {
      while ((rp->nextid) && (id >= rp->nextid->id))
	rp = rp->nextid;
      while ((rp->previousid) && (id <= rp->previousid->id))
	rp = rp->previousid;
      if (id == rp->id)
	{
	  idbash[bashnum] = rp;
	  return rp;
	}
      else
	return NULL;
    }
  return rp;			/* NULL */
}


struct resolve *
findip (ip_t ip)
{
  struct resolve *rp;
  dword bashnum;
  bashnum = getipbash (ip);
  rp = ipbash[bashnum];
  if (rp)
    {
      while ((rp->nextip) && (ip >= rp->nextip->ip))
	rp = rp->nextip;
      while ((rp->previousip) && (ip <= rp->previousip->ip))
	rp = rp->previousip;
      if (ip == rp->ip)
	{
	  ipbash[bashnum] = rp;
	  return rp;
	}
      else
	return NULL;
    }
  return rp;			/* NULL */
}

void
restell (char *s)
{
  /* TODO JTC Turn this into a g_my_debug or something */
#if Debug
  fputs (s, stderr);
  fputs ("\r", stderr);
#endif
}

void
dorequest (char *s, int type, word id)
{
  packetheader *hp;
  int r, i;
  int buf[(MaxPacketsize / sizeof (int)) + 1];

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

/* sends a request asking name from addr */
void
resendrequest_inverse (struct resolve *rp)
{
  switch (rp->state)
    {
    case STATE_FINISHED:
    case STATE_FAILED:
      /** DNS answered, reply expired, this is a refresh request, it's enough to use a
          small timeout 
          Note: a DNS "not found" is  considered a valid answer in this context
      */
      calc_expire_tick (rp, 18, 1);	/* 3 minutes initial expire time */
      break;

    default:
      /* every other state means: DSN not responding, we need to escalate the refresh time, 
         to reduce the number of packets */
      calc_expire_tick (rp, rp->last_expire_tick * 2, 1);	/* doubling refresh time */
      break;
    }

  /* sending the request */
  rp->state = STATE_PTRREQ;
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

/* creates a new item and asks the DNS for its name */
void
sendrequest_inverse (ip_t ip)
{
  struct resolve *rp = NULL;

  /* allocate and fill the new item */
  rp = allocresolve ();
  rp->state = STATE_PTRREQ;
  rp->ip = ip;
  linkresolveip (rp);

  /* create an id uniquely identifiyng the new item - this id will be used to match the DNS response
     with the item */
  do
    {
      idseed =
	(((idseed + idseed) | (long) time (NULL)) + idseed -
	 0x54bad4a) ^ aseed;
      aseed ^= idseed;
      rp->id = (word) idseed;
    }
  while (findid (rp->id));
  linkresolveid (rp);

  resendrequest_inverse (rp);
}

/* DNS answer: not found */
void
failrp (struct resolve *rp)
{
  if (rp->state == STATE_FINISHED)
    return;			/* already received a good response - ignore failure */
  rp->state = STATE_FAILED;

  /* will retry after a day */
  calc_expire_tick (rp, 8640, 0);

  link_activelist (rp);		/* item becomes head of active list */
  if (debug)
    restell ("Resolver: Lookup failed.\n");
}

/* DNS answer: found addr */
void
passrp (struct resolve *rp, long ttl)
{
  rp->state = STATE_FINISHED;
  /** using ttl/2 to calculate an expire tick (need to divide by 6 to convert to ticks) 
     Note:  
     TTL is expressed in seconds
   */
  calc_expire_tick (rp, ttl / 12, 0);

  /* extracting the base name from fqdn */
  if (rp->fq_hostname && !rp->only_hostname)
    {
      char *tmp;
      tmp = strpbrk (rp->fq_hostname, ".");
      if (tmp)
	{
	  rp->only_hostname = (char *) malloc ((tmp - rp->fq_hostname) + 1);
	  if (rp->only_hostname)
	    {
	      /* copies only basename */
	      strncpy (rp->only_hostname, rp->fq_hostname,
		       tmp - rp->fq_hostname);
	      rp->only_hostname[tmp - rp->fq_hostname] = '\0';
	    }
	}
    }

  link_activelist (rp);		/* item becomes head of active list */
  if (debug)
    {
      snprintf (tempstring, sizeof (tempstring),
		"Resolver: Lookup successful: %s\n", rp->fq_hostname);
      restell (tempstring);
    }
}

/* handles the NOERROR response */
void
parserespacket_noerror (byte * s, int l, struct resolve *rp,
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

  if (Is_PTR (rp))
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
      if (!Is_PTR (rp))
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
		if (memcmp (&rp->ip, (ip_t *) c, sizeof (ip_t)))
		  {
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver: Reverse authentication failed: %s != ",
			      strlongip (rp->ip));
		    memcpy (&alignedip, (ip_t *) c, sizeof (ip_t));
		    safe_strncat (tempstring, strlongip (alignedip),
				  sizeof (tempstring));
		    restell (tempstring);
		    res_hostipmismatch++;
		    failrp (rp);
		  }
		else
		  {
		    snprintf (tempstring, sizeof (tempstring),
			      "Resolver: Reverse authentication complete: %s == \"%s\".",
			      strlongip (rp->ip),
			      (rp->fq_hostname) ? rp->fq_hostname : "");
		    restell (tempstring);
		    res_reversesuccess++;
		    passrp (rp, ttl);
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
		    failrp (rp);
		    return;
		  }
		if (datatype == T_CNAME)
		  {
		    safe_strncpy (stackstring, namestring,
				  sizeof (stackstring));
		    break;
		  }
		if (!rp->fq_hostname)
		  {
		    rp->fq_hostname =
		      (char *) malloc (strlen (namestring) + 1);
		    if (!rp->fq_hostname)
		      {
			fprintf (stderr, "malloc() error: %s\n",
				 strerror (errno));
			exit (-1);
		      }
		    strcpy (rp->fq_hostname, namestring);
		    passrp (rp, ttl);
		    res_iplookupsuccess++;
		  }
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
void
parserespacket (byte * s, int l)
{
  struct resolve *rp;
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
  rp = findid (hp->id);
  if (!rp)
    {
      res_unknownid++;
      return;
    }
  if ((rp->state == STATE_FINISHED) || (rp->state == STATE_FAILED))
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
      failrp (rp);
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
dns_ack ()
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
	parserespacket ((byte *) resrecvbuf, r);
    }
  else
    {
      snprintf (tempstring, sizeof (tempstring), "Resolver: Socket error: %s",
		strerror (errno));
      restell (tempstring);
    }
}

char *
dns_lookup2 (ip_t ip, int fqdn)
{
  struct resolve *rp;
  ip = htonl (ip);
  if ((rp = findip (ip)))
    {
      /* item found, if expired resend the request */
      if (is_expired_tick (rp->expire_tick))
	resendrequest_inverse (rp);

      if ((rp->state == STATE_FINISHED) || (rp->state == STATE_FAILED))
	{
	  /* item existing, make it the head of active list */
	  link_activelist (rp);

	  if ((rp->state == STATE_FINISHED) && (rp->fq_hostname))
	    {
	      /* record found */
	      if (debug)
		{
		  snprintf (tempstring, sizeof (tempstring),
			    "Resolver: Used cached record: %s == \"%s\".\n",
			    strlongip (ip), rp->fq_hostname);
		  restell (tempstring);
		}
	      /* return fqdn if requested or basename missing */
	      if (fqdn || !rp->only_hostname)
		return rp->fq_hostname;
	      else
		return rp->only_hostname;
	    }
	  else
	    {
	      /* still no name */
	      if (debug)
		{
		  snprintf (tempstring, sizeof (tempstring),
			    "Resolver: Used failed record: %s == ???\n",
			    strlongip (ip));
		  restell (tempstring);
		}
	      return NULL;
	    }
	}
      return NULL;
    }

  /* new record - making a request */
  if (debug)
    fprintf (stderr, "Resolver: Added to new record.\n");
  sendrequest_inverse (ip);
  return NULL;
}


char *
dns_lookup (ip_t ip, int fqdn)
{
  char *t;
  if (!dns)
    return strlongip (ip);
  t = dns_lookup2 (ip, fqdn);

  if (!t)
    return strlongip (ip);

  return t;
}
