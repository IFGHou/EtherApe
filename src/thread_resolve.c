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
#include <ip-cache.h>
#include <thread_resolve.h>


#define ETHERAPE_THREAD_POOL_SIZE 6

/* to-resolve-items linked list */
struct ipresolve_link
{
  struct ipcache_item *itemToResolve;
  struct ipresolve_link *next;   
};
static struct ipresolve_link *resolveListHead=NULL;  
static struct ipresolve_link *resolveListTail=NULL;  



uint32_t alignedip;
static int request_stop_thread = 0; /* stop thread flag */

static void
dbgprint (const char *s, ...)
{
#if Debug
  va_list ap;
  va_start(ap, l);
  vfprintf(stderr, s, ap);
  va_end(ap);
#endif
}


/* mutexes */

static pthread_mutex_t resolvemtx; /* resolve mutex */
static pthread_cond_t resolvecond; /* resolve condition var */

static void
open_mutex(void)
{
    pthread_mutex_init (&resolvemtx, NULL);
    pthread_cond_init (&resolvecond, NULL);
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
   struct in_addr addr;
   struct hostent resultbuf;
   struct hostent *resultptr;
   char extrabuf[4096];
   int errnovar;
   int result;

   while (!request_stop_thread)
   {
      pthread_mutex_lock(&resolvemtx);
      if (!resolveListHead)
      {
         /* list empty, wait on condition releasing mutex */
         pthread_cond_wait (&resolvecond, &resolvemtx);
      }

      /* if we came here the mutex is locked and head should be filled,
         but a check doesn't hurt ... */
      if (resolveListHead)
      {
         /* there is something to resolve, take out from head */
         el = resolveListHead;
         resolveListHead = el->next;
         curitem = el->itemToResolve;
         
         /* ok, release mutex and resolve */
         pthread_mutex_unlock(&resolvemtx);

         free(el); /* item saved, link elem now useless, freeing */
         
         addr.s_addr = htonl (curitem->ip);
         result = gethostbyaddr_r (&addr, sizeof(addr), AF_INET, 
                          &resultbuf, extrabuf, sizeof(extrabuf), 
                          &resultptr, &errnovar);
         
         if (result != 0)
            dbgprint("Insufficient memory allocated to gethostbyaddr_r\n");

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

static void
start_threads()
{
   pthread_t curth;
   pthread_attr_t attr;
   int i;
   
   /* prepare thread data */
   pthread_attr_init (&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

   /* reset stop flag */
   request_stop_thread = 0;
   
   for (i=0; i<ETHERAPE_THREAD_POOL_SIZE ; ++i)
   {
       if (pthread_create ( &curth, &attr, thread_pool_routine, NULL))
       {
          // error
       }
   }
}

static void
stop_threads()
{
   request_stop_thread = 1;
   pthread_cond_broadcast(&resolvecond); /* wake all threads */
}

/* creates a request, placing in the queue 
   NOTE: mutex locked!
*/
static void
sendrequest_inverse (uint32_t ip)
{
  struct ipcache_item *rp = NULL;

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

  dbgprint("Resolver: queued request \"%s\".", strlongip (rp->ip));
}



int
thread_waitfd (void)
{
  return -1;
}

/* called to activate the resolver */
void
thread_open (void)
{
  /* mutex creation */ 
  open_mutex();

  /* cache activation */  
  ipcache_init();

  /* thread pool activation */ 
  start_threads();
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

/* returns 1 if the current dns implementation has a socket wich needs a select() */
int thread_hasfd(void)
{
   return 0;
}

/* called if the socket has some data */
void
thread_ack ()
{
}

char *
thread_lookup (uint32_t ip, int fqdn)
{
  char *ipname;
  int is_expired = 0;
  
  /* locks mutex */ 
  pthread_mutex_lock(&resolvemtx);
   
  /* asks cache */
  ipname = ipcache_getnameip(ip, fqdn, &is_expired);

  if (is_expired)
      sendrequest_inverse (ip); /* request needed */ 
    
  /* release mutex and exit */
  pthread_mutex_unlock(&resolvemtx);
  return ipname;
}
