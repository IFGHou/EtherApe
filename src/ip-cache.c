/*
   $Id$

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
   
   ----------------------------------------------------------------
   
   ip-caching routines
   This simple LRU cache contains both resonveld and un-resolved ip addresses
   It's primarily targeted to use by the dns resolver, to avoid unneccessary requests
   Found names expiry from cache after approx TTL/2 days, with a maximum of ten (10)
   Not-found names timeout doubles with every unsatisfiend request, up to a maximum of ten days.
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
#include "globals.h"
#include "ip-cache.h"
#include "util.h"

#ifdef NO_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(errno) (((errno) >= 0 && (errno) < sys_nerr) ? sys_errlist[errno] : "unlisted error")
#endif

/* Defines */

#define BashSize 8192		/* Size of hash tables */
#define BashModulo(x) ((x) & 8191)	/* Modulo for hash table size: */


struct ipcache_item *idbash[BashSize];
struct ipcache_item *ipbash[BashSize];

/* max cache handling ... */
static struct ipcache_item *active_list = NULL;	/* list of active item for purging */
static long num_active = 0;
static long max_active = 1024;

long idseed = 0xdeadbeef;
long aseed;

/* internal funcs fwd decls */
static void ipcache_calc_expire_tick (struct ipcache_item *rp, unsigned long delay, int is_request);
static void ipcache_unlink_activelist (struct ipcache_item *rp);
static void ipcache_unlinkresolve (struct ipcache_item *rp);

/* 
	-----------------------------------------
	common utility functions (todo: move to another file)
	-----------------------------------------   
*/


/* converts a long containing a time value expressed in second to a (passed) string buffer and returns it */
char *
strtdiff (char *d, size_t lend, long signeddiff)
{
  unsigned long diff;
  unsigned long seconds, minutes, hours;
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

char *
strlongip (uint32_t ip)
{
  struct in_addr a;
  a.s_addr = htonl (ip);
  return inet_ntoa (a);
}

/* 
	-----------------------------------------
	tick calculation functions
	-----------------------------------------   
*/



/* current tick */
static unsigned long current_tick = 0;

/* called to signal a new tick
   Note:
   No provisions made to prevent counter wrapping. With a 32-bit counter and a tick 
   of 10secs, the counter will wrap every 1300 years or so of continuous running.
*/
unsigned int
ipcache_tick (void)
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
static void
ipcache_calc_expire_tick (struct ipcache_item *rp, unsigned long delay, int is_request)
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
static int
ipcache_is_expired_tick (unsigned long tick)
{
  return (tick < current_tick);
}

/* 
	-----------------------------------------
	cache functions
	-----------------------------------------   
*/


/* initializes the cache */
void
ipcache_init(void)
{
  int i;
  aseed = time (NULL) ^ (time (NULL) << 3) ^ (unsigned short) getpid ();
  for (i = 0; i < BashSize; i++)
    {
      idbash[i] = NULL;
      ipbash[i] = NULL;
    }
}

long ipcache_active_entries(void)
{
  return num_active;
}

static unsigned short
ipcache_getidbash (unsigned short id)
{
  return (unsigned short) BashModulo (id);
}

static unsigned short
ipcache_getipbash (uint32_t ip)
{
  return (unsigned short) BashModulo (ip);
}

/* removes rp from the active list */
static void
ipcache_unlink_activelist (struct ipcache_item *rp)
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
ipcache_purge_activelist (void)
{
  if (num_active > max_active && num_active > 10)
    {
      /* too much items, purge oldest items until we have some room */
      while (active_list && num_active > 0 && num_active >= max_active - 10)
	{
	  struct ipcache_item *rp = active_list->previous_active;
	  ipcache_unlinkresolve (rp);
	  free (rp);
	}
    }
}

/* adds or moves rp to head of the active list */
static void
ipcache_link_activelist (struct ipcache_item *rp)
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
	    ipcache_purge_activelist ();	/* new element, make sure there is room ... */
	  else
	    ipcache_unlink_activelist (rp);	/* moving element, first unlink, then relink */

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

static void
ipcache_linkresolveid (struct ipcache_item *addrp)
{
  struct ipcache_item *rp;
  unsigned short bashnum;
  bashnum = ipcache_getidbash (addrp->id);
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

static void
ipcache_unlinkresolveid (struct ipcache_item *rp)
{
  unsigned short bashnum;
  bashnum = ipcache_getidbash (rp->id);
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

static void
ipcache_linkresolveip (struct ipcache_item *addrp)
{
  struct ipcache_item *rp;
  unsigned short bashnum;
  bashnum = ipcache_getipbash (addrp->ip);
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

static void
ipcache_unlinkresolveip (struct ipcache_item *rp)
{
  unsigned short bashnum;
  bashnum = ipcache_getipbash (rp->ip);
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


static void
ipcache_unlinkresolve (struct ipcache_item *rp)
{
  ipcache_unlink_activelist (rp);	/* removes from purge list */
  ipcache_unlinkresolveid (rp);
  ipcache_unlinkresolveip (rp);

  if (rp->fq_hostname)
    free (rp->fq_hostname);
}

struct ipcache_item *
ipcache_findid (unsigned short id)
{
  struct ipcache_item *rp;
  int bashnum;
  bashnum = ipcache_getidbash (id);
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


static struct ipcache_item *
ipcache_findip (uint32_t ip)
{
  struct ipcache_item *rp;
  unsigned short bashnum;
  bashnum = ipcache_getipbash (ip);
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

static struct ipcache_item *
ipcache_alloc_item (uint32_t ip)
{
  struct ipcache_item *rp;
  rp = (struct ipcache_item *) malloc (sizeof (struct ipcache_item));
  if (!rp)
    {
      fprintf (stderr, "malloc() failed: %s\n", strerror (errno));
      exit (-1);
    }
  memset (rp, 0, sizeof (struct ipcache_item));

  rp->state = IPCACHE_STATE_PTRREQ;
  rp->ip = ip;
  ipcache_linkresolveip (rp);

  /* create an id uniquely identifiyng the new item - this id will be used to match the DNS response
     with the item */
  do
    {
      idseed =
	(((idseed + idseed) | (long) time (NULL)) + idseed -
	 0x54bad4a) ^ aseed;
      aseed ^= idseed;
      rp->id = (unsigned short) idseed;
    }
  while (ipcache_findid (rp->id));
  ipcache_linkresolveid (rp);

  return rp;
}

/* prepares a request for the specified ip address */
struct ipcache_item *
ipcache_prepare_request(uint32_t ip)
{
  struct ipcache_item *rp = NULL;

  if (!(rp = ipcache_findip(ip)))
     rp = ipcache_alloc_item (ip); /* ip not found, allocate and fill a new item */

  /* item prepared, fill data */
  switch (rp->state)
    {
    case IPCACHE_STATE_FINISHED:
    case IPCACHE_STATE_FAILED:
      /** DNS answered, reply expired, this is a refresh request, it's enough to use a
          small timeout 
          Note: a DNS "not found" is  considered a valid answer in this context
      */
      ipcache_calc_expire_tick (rp, 18, 1);	/* 3 minutes initial expire time */
      break;

    default:
      /* every other state means: DSN not responding, we need to escalate the refresh time, 
         to reduce the number of packets */
      ipcache_calc_expire_tick (rp, rp->last_expire_tick * 2, 1);	/* doubling refresh time */
      break;
    }

  /* new state: sent request */
  rp->state = IPCACHE_STATE_PTRREQ;
  return rp;
}

/* DNS answer: not found */
void
ipcache_request_failed(struct ipcache_item *rp)
{
  if (rp->state == IPCACHE_STATE_FINISHED)
    return;			/* already received a good response - ignore failure */
  rp->state = IPCACHE_STATE_FAILED;

  /* will retry after a day */
  ipcache_calc_expire_tick (rp, 8640, 0);

  ipcache_link_activelist (rp);		/* item becomes head of active list */
}

/* DNS answer: found addr */
void
ipcache_request_succeeded(struct ipcache_item *rp, long ttl, char *ipname)
{
  rp->state = IPCACHE_STATE_FINISHED;
  /** using ttl/2 to calculate an expire tick (need to divide by 6 to convert to ticks) 
     Note:  
     TTL is expressed in seconds
   */
  ipcache_calc_expire_tick (rp, ttl / 12, 0);

  /* first, copies the resolved (fqdn) name */ 
  if (ipname)
     {
        rp->fq_hostname = (char *) malloc (strlen (ipname) + 1);
        if (!rp->fq_hostname)
          {
            fprintf (stderr, "malloc() error: %s\n",
                     strerror (errno));
            exit (-1);
          }
        strcpy (rp->fq_hostname, ipname); /* safe - buffer allocated */
    }

  ipcache_link_activelist (rp);		/* item becomes head of active list */
}

/* returns the name corresponding to the supplied ip addr or a formatted ip-addr
if isn't resolved. 
on exit is_expired contains true if the record is expired and must be refreshed 
*/
const char *
ipcache_getnameip(uint32_t ip, int *is_expired)
{
  struct ipcache_item *rp;
  uint32_t iptofind = ip;

  if (!pref.name_res)
    return strlongip (ip); /* name resolution globally disabled */

  /*iptofind = htonl (ip);*/
  
  if ((rp = ipcache_findip (iptofind)))
    {
      /* item found, if expired set the flag */ 
      if (ipcache_is_expired_tick (rp->expire_tick))
         *is_expired = 1; 
      else
         *is_expired = 0;

      if ((rp->state == IPCACHE_STATE_FINISHED) || (rp->state == IPCACHE_STATE_FAILED))
	{
	  /* item existing, make it the head of active list */
	  ipcache_link_activelist (rp);

	  if ((rp->state == IPCACHE_STATE_FINISHED) && (rp->fq_hostname))
	    {
	      /* item already resolved, return fqdn */
              return rp->fq_hostname;
	    }
	} 
    }
  else
     *is_expired = 1; /* item not found - needs request */
    
   /* item not found or still without name */ 
   return strlongip(ip);
}

/* fully clear the cache */
void ipcache_clear(void)
{
    while (active_list && num_active > 0)
      {
        struct ipcache_item *rp = active_list->previous_active; // tail
        ipcache_unlinkresolve (rp);
        free (rp);
      }
}
