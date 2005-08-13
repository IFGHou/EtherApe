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
   
   ----------------------------------------------------------------
   
   ip-caching routines
*/

/*  Prototypes for ip-cache.c  */

enum IPCACHE_STATE
{
  IPCACHE_STATE_FINISHED,		/* query successful */
  IPCACHE_STATE_FAILED,			/* query failed */
  IPCACHE_STATE_PTRREQ			/* PTR query packet sent */
} ;

struct ipcache_item
{
  /* these linked lists are used for hash table overflow */
  struct ipcache_item *nextid;
  struct ipcache_item *previousid;
  struct ipcache_item *nextip;
  struct ipcache_item *previousip;

  /* linked list of struct for purging */
  struct ipcache_item *next_active;
  struct ipcache_item *previous_active;

  unsigned long expire_tick;	/* when current_tick > expire_tick this node is expired */
  unsigned long last_expire_tick;	/* used to escalate old timers ... */
  char *fq_hostname;		/* fully qualified hostname */
  char *only_hostname;		/* hostname without domain */
  uint32_t ip;			/* ip addr */
  unsigned short id;			/* unique item id */
  enum IPCACHE_STATE state;			/* current state */
};


void ipcache_init (void);
unsigned int ipcache_tick (void);	/* call this more or less every 10 secs */

struct ipcache_item *ipcache_prepare_request(unsigned int ip);
char *ipcache_getnameip(uint32_t ip, int fqdn, int *is_expired);
void ipcache_request_failed(struct ipcache_item *rp);
void ipcache_request_succeeded(struct ipcache_item *rp, long ttl, char *ipname);
struct ipcache_item *ipcache_findid (unsigned short id);


char *safe_strncpy (char *dst, const char *src, size_t maxlen);
char *safe_strncat (char *dst, const char *src, size_t maxlen);
char *strtdiff (char *d, size_t lend, long signeddiff);
char *strlongip (unsigned int ip);
