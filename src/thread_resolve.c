/*
   Etherape
   Copyright (C) 2005 R.Ghetta

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
   These routines use multiple threads to call gethostbyaddr() and resolve
   names. All thread handling is internal.
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include <pthread.h>
#include "common.h"
#include "ip-cache.h"
#include "thread_resolve.h"

#define ETHERAPE_THREAD_POOL_SIZE 6
static pthread_t resolver_threads[ETHERAPE_THREAD_POOL_SIZE];
static int resolver_threads_num = 0;

/* to-resolve-items linked list */
struct ipresolve_link
{
  struct ipcache_item *itemToResolve;
  struct ipresolve_link *next;   
};
static struct ipresolve_link *resolveListHead=NULL;  
static struct ipresolve_link *resolveListTail=NULL;  

static int request_stop_thread = 0; /* stop thread flag */

/* mutexes */

static pthread_mutex_t resolvemtx; /* resolve mutex */
static pthread_cond_t resolvecond; /* resolve condition var */

static int open_mutex(void)
{
    if (!pthread_mutex_init (&resolvemtx, NULL) &&
        !pthread_cond_init (&resolvecond, NULL))
      return 0; /* ok*/
    else
      return 1; 
}

static void
close_mutex(void)
{
    pthread_mutex_destroy(&resolvemtx);
    pthread_cond_destroy(&resolvecond);
}

/* thread routine */
static void *
thread_pool_routine(void *dt)
{
   struct ipcache_item *curitem;
   struct ipresolve_link *el;   
   struct hostent resultbuf;
   struct hostent *resultptr;
   char extrabuf[4096];
   int errnovar;
   int result;

   while (!request_stop_thread)
   {
      pthread_mutex_lock(&resolvemtx);

      if (!request_stop_thread && !resolveListHead)
      {
         /* list empty, wait on condition releasing mutex */
         pthread_cond_wait (&resolvecond, &resolvemtx);
      }

      /* from now on, mutex is locked */

      if (request_stop_thread)
      {
         /* must exit */
         pthread_mutex_unlock(&resolvemtx);
         break;
      }

      /* if we come here head should be filled, but a check doesn't hurt ... */
      if (resolveListHead)
      {
         /* there is something to resolve, take out from head */
         el = resolveListHead;
         resolveListHead = el->next;
         curitem = el->itemToResolve;
         
         /* ok, release mutex and resolve */
         pthread_mutex_unlock(&resolvemtx);

         free(el); /* item saved, link elem now useless, freeing */

#ifdef FORCE_SINGLE_THREAD
         /* if forced single thread, uses gethostbyaddr */
         result=0;
         resultptr = gethostbyaddr (&curitem->ip.addr8, 
                          address_len(curitem->ip.type), curitem->ip.type);
#else
         /* full multithreading, use thread safe gethostbyaddr_r */
         result = gethostbyaddr_r (&curitem->ip.addr8, 
                          address_len(curitem->ip.type), curitem->ip.type, 
                          &resultbuf, extrabuf, sizeof(extrabuf), 
                          &resultptr, &errnovar);
         if (result != 0 && errnovar == ERANGE)
            g_my_critical("Insufficient memory allocated to gethostbyaddr_r\n");
#endif
         if (request_stop_thread)
            break;

         /* resolving completed or failed, lock again and notify ip-cache */
         pthread_mutex_lock(&resolvemtx);
         if (result || !resultptr)
            ipcache_request_failed(curitem); 
         else
            ipcache_request_succeeded(curitem, 3600L*24L, resultptr->h_name);
      }

      pthread_mutex_unlock(&resolvemtx);
   }
   return NULL;
}

static void start_threads()
{
   pthread_t curth;
   int i;
   int maxth = ETHERAPE_THREAD_POOL_SIZE;
   pthread_attr_t attr;

   /* reset stop flag */
   request_stop_thread = 0;

   if (pthread_attr_init(&attr) ||
       pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
     {
       g_critical("pthread_attr_init failed, resolver will not be available\n");
       return; 
     }
  
  /* if single thread resolving is forced, then start only a single thread,
     else start ETHERAPE_THREAD_POOL_SIZE threads */
#ifdef FORCE_SINGLE_THREAD
   maxth = 1;
#endif
   for (i=0; i<maxth ; ++i)
   {
     if (pthread_create ( &curth, NULL, thread_pool_routine, NULL))
     {
       // error, stop creating threads
       g_critical("pthread_create failed, resolver has only %d threads\n", i);
       break;
     }
     resolver_threads[i] = curth;
   }

   resolver_threads_num = i;
}

static void stop_threads()
{
  /* take mutex, to make sure other thread will be waiting */
  pthread_mutex_lock(&resolvemtx);

  /* activate variable */
  request_stop_thread = 1;
  pthread_cond_broadcast(&resolvecond); /* wake all threads */
  pthread_mutex_unlock(&resolvemtx);

  resolver_threads_num = 0;
}

/* creates a request, placing in the queue 
   NOTE: mutex locked!
*/
static void
sendrequest_inverse (address_t *ip)
{
  struct ipcache_item *rp = NULL;

  if (!ip)
      return;

  /* allocate a new request */
  rp = ipcache_prepare_request(ip);

  /* prepare resolve item */
  struct ipresolve_link *newitm= (struct ipresolve_link *)malloc( sizeof(struct ipresolve_link));
  newitm->itemToResolve = rp;
  newitm->next= NULL;

  /* items placed on list tail and consumed from head (this gives a FIFO strategy) */
  if (NULL == resolveListHead)
  {
     /* first item, we put head=tail */
     resolveListTail = resolveListHead = newitm;
  }
  else
  {
     resolveListTail->next = newitm;
     resolveListTail = newitm;
  }
  
  /* signal the condition and release the mux */
  pthread_cond_signal(&resolvecond);

  g_my_debug("Resolver: queued request \"%s\".", strlongip (&rp->ip));
}

/* called to activate the resolver */
int 
thread_open (void)
{
  /* mutex creation */ 
  if (open_mutex())
    return 1;

  /* cache activation */  
  ipcache_init();

  /* thread pool activation */ 
  start_threads();

  return 0;
}

/* called to close the resolver */
void
thread_close(void)
{
  /* thread pool shutdown */ 
  stop_threads();
   
  /* close mutex */ 
  close_mutex();
}

const char *
thread_lookup (address_t *ip)
{
  const char *ipname;
  int is_expired = 0;

  if (!ip)
      return "";

  /* locks mutex */ 
  pthread_mutex_lock(&resolvemtx);
   
  /* asks cache */
  ipname = ipcache_getnameip(ip, &is_expired);

  if (is_expired)
      sendrequest_inverse (ip); /* request needed */ 
    
  /* release mutex and exit */
  pthread_mutex_unlock(&resolvemtx);
  return ipname;
}

