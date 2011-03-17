/* resolv.c
 * Routines for network object lookup
 * $Id$
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * Originally written by Laurent Deniel <deniel@worldnet.fr>
 * Adapted to etherape by Juan Toledo <toledo@users.sourceforge.net>
 * Further changes by Riccardo Ghetta <bchiara@users.sourceforge.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <signal.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef NEED_INET_V6DEFS_H
#include "inet_v6defs.h"
#endif

#include <glib.h> 
#include "appdata.h"
#include "util.h"

#define EPATH_ETHERS 		"/etc/ethers"
#define EPATH_MANUF  		DATAFILE_DIR "/manuf"
#define EPATH_PERSONAL_ETHERS 	".etherape/ethers"  /* with "$HOME/" prefix */	/* JTC */

#ifndef MAXNAMELEN
#define MAXNAMELEN  	64	/* max name length (hostname and port name) */
#endif


#define MAXMANUFLEN	9	/* max vendor name length with ending '\0' */
#define HASHETHSIZE	1024
#define HASHMANUFSIZE   256
#define HASHPORTSIZE	256

/* hash tables used for ethernet and manufacturer lookup */

typedef struct hashmanuf
{
  u_char addr[3];
  char name[MAXMANUFLEN];
  struct hashmanuf *next;
}
hashmanuf_t;

typedef struct hashether
{
  u_char addr[6];
  char name[MAXNAMELEN];
  gboolean is_name_from_file;
  struct hashether *next;
}
hashether_t;

/* internal ethernet type */

typedef struct _ether
{
  u_char addr[6];
  char name[MAXNAMELEN];
}
ether_t;

static hashmanuf_t *manuf_table[HASHMANUFSIZE];
static hashether_t *eth_table[HASHETHSIZE];

static int eth_resolution_initialized = 0;

/*
 *  Global variables (original impl. changed them in GUI sections)
 */

static gchar *g_ethers_path = EPATH_ETHERS;
static gchar *g_pethers_path = NULL;	/* "$HOME"/EPATH_PERSONAL_ETHERS    */
static gchar *g_manuf_path = EPATH_MANUF;	/* may only be changed before the   */
					/* first resolving call             */

/*
 *  Miscellaneous functions
 */

static int
fgetline (char **buf, int *size, FILE * fp)
{
  int len;
  int c;

  if (fp == NULL)
    return -1;

  if (*buf == NULL)
    {
      if (*size == 0)
	*size = BUFSIZ;

      if ((*buf = g_malloc (*size)) == NULL)
	return -1;
    }

  if (feof (fp))
    return -1;

  len = 0;
  while ((c = getc (fp)) != EOF && c != '\n')
    {
      if (len + 1 >= *size)
	{
          *size += BUFSIZ;
	  if ((*buf = g_realloc (*buf, *size)) == NULL)
	    return -1;
	}
      (*buf)[len++] = c;
    }

  if (len == 0 && c == EOF)
    return -1;

  (*buf)[len] = '\0';

  return len;

}				/* fgetline */


/*
 * Ethernet / manufacturer resolution
 *
 * The following functions implement ethernet address resolution and
 * ethers files parsing (see ethers(4)). 
 *
 * /etc/manuf has the same format as ethers(4) except that names are 
 * truncated to MAXMANUFLEN-1 characters and that an address contains 
 * only 3 bytes (instead of 6).
 *
 * Notes:
 *
 * I decide to not use the existing functions (see ethers(3) on some 
 * operating systems) for the following reasons:
 * - performance gains (use of hash tables and some other enhancements),
 * - use of two ethers files (system-wide and per user),
 * - avoid the use of NIS maps,
 * - lack of these functions on some systems.
 *
 * So the following functions do _not_ behave as the standard ones.
 *
 * -- Laurent.
 */


static int
parse_ether_line (char *line, ether_t * eth, int six_bytes)
{
  /*
   *  See man ethers(4) for /etc/ethers file format
   *  (not available on all systems).
   *  We allow both ethernet address separators (':' and '-'),
   *  as well as Ethereal's '.' separator.
   */

  gchar *cp;
  unsigned int a0, a1, a2, a3, a4, a5;

  if ((cp = strchr (line, '#')))
    *cp = '\0';

  if ((cp = strtok (line, " \t\n")) == NULL)
    return -1;

  if (six_bytes)
    {
      if (sscanf (cp, "%x:%x:%x:%x:%x:%x", &a0, &a1, &a2, &a3, &a4, &a5) != 6)
	{
	  if (sscanf (cp, "%x-%x-%x-%x-%x-%x", &a0, &a1, &a2, &a3, &a4, &a5)
	      != 6)
	    {
	      if (sscanf
		  (cp, "%x.%x.%x.%x.%x.%x", &a0, &a1, &a2, &a3, &a4,
		   &a5) != 6)
		return -1;
	    }
	}
    }
  else
    {
      if (sscanf (cp, "%x:%x:%x", &a0, &a1, &a2) != 3)
	{
	  if (sscanf (cp, "%x-%x-%x", &a0, &a1, &a2) != 3)
	    {
	      if (sscanf (cp, "%x.%x.%x", &a0, &a1, &a2) != 3)
		return -1;
	    }
	}
    }

  if ((cp = strtok (NULL, " \t\n")) == NULL)
    return -1;

  eth->addr[0] = a0;
  eth->addr[1] = a1;
  eth->addr[2] = a2;
  if (six_bytes)
    {
      eth->addr[3] = a3;
      eth->addr[4] = a4;
      eth->addr[5] = a5;
    }
  else
    {
      eth->addr[3] = 0;
      eth->addr[4] = 0;
      eth->addr[5] = 0;
    }

  safe_strncpy (eth->name, cp, MAXNAMELEN);

  return 0;

}				/* parse_ether_line */

static FILE *eth_p = NULL;

static void
set_ethent (char *path)
{
  if (eth_p)
    rewind (eth_p);
  else
    eth_p = fopen (path, "r");
}

static void
end_ethent (void)
{
  if (eth_p)
    {
      fclose (eth_p);
      eth_p = NULL;
    }
}

static ether_t *
get_ethent (int six_bytes)
{

  static ether_t eth;
  static int size = 0;
  static char *buf = NULL;

  if (eth_p == NULL)
    return NULL;

  while (fgetline (&buf, &size, eth_p) >= 0)
    {
      if (parse_ether_line (buf, &eth, six_bytes) == 0)
	{
	  return &eth;
	}
    }

  return NULL;

}				/* get_ethent */

static const ether_t *
get_ethbyaddr (const u_char * addr)
{

  ether_t *eth;

  set_ethent (g_ethers_path);

  while ((eth = get_ethent (1)) && memcmp (addr, eth->addr, 6) != 0)
    ;

  if (eth == NULL)
    {
      end_ethent ();

      set_ethent (g_pethers_path);

      while ((eth = get_ethent (1)) && memcmp (addr, eth->addr, 6) != 0)
	;

      end_ethent ();
    }

  return eth;

}				/* get_ethbyaddr */

static void
add_manuf_name (u_char * addr, char * name)
{

  hashmanuf_t *tp;
  hashmanuf_t **table = manuf_table;

  tp = table[((int) addr[2]) & (HASHMANUFSIZE - 1)];

  if (tp == NULL)
    {
      tp = table[((int) addr[2]) & (HASHMANUFSIZE - 1)] =
	(hashmanuf_t *) g_malloc (sizeof (hashmanuf_t));
      g_assert(tp);
    }
  else
    {
      while (1)
	{
	  if (tp->next == NULL)
	    {
	      tp->next = (hashmanuf_t *) g_malloc (sizeof (hashmanuf_t));
              g_assert(tp->next);
	      tp = tp->next;
	      break;
	    }
	  tp = tp->next;
	}
    }

  memcpy (tp->addr, addr, sizeof (tp->addr));
  safe_strncpy (tp->name, name, MAXMANUFLEN);
  tp->next = NULL;

}				/* add_manuf_name */

static const hashmanuf_t *
manuf_name_lookup (const u_char * addr)
{
  hashmanuf_t *tp;
  hashmanuf_t **table = manuf_table;

  tp = table[((int) addr[2]) & (HASHMANUFSIZE - 1)];

  while (tp != NULL)
    {
      if (memcmp (tp->addr, addr, sizeof (tp->addr)) == 0)
	{
	  return tp;
	}
      tp = tp->next;
    }

  return NULL;

}				/* manuf_name_lookup */

static void
initialize_ethers (void)
{
  ether_t *eth;

#ifdef DEBUG_RESOLV
  signal (SIGSEGV, SIG_IGN);
#endif

  /* Set g_pethers_path here, but don't actually do anything
   * with it. It's used in get_ethbyname() and get_ethbyaddr()
   */
  if (g_pethers_path == NULL)
    {
      g_pethers_path=g_strdup_printf ("%s/%s",
	       get_home_dir (), EPATH_PERSONAL_ETHERS);
    }

  /* manuf hash table initialization */

  set_ethent (g_manuf_path);

  while ((eth = get_ethent (0)))
    {
      add_manuf_name (eth->addr, eth->name);
    }

  end_ethent ();

}				/* initialize_ethers */

static const char *
eth_name_lookup (const u_char * addr, gboolean only_ethers)
{
  const hashmanuf_t *manufp;
  hashether_t *tp;
  hashether_t **table = eth_table;
  const ether_t *eth;
  int i, j;

  j = (addr[2] << 8) | addr[3];
  i = (addr[4] << 8) | addr[5];

  tp = table[(i ^ j) & (HASHETHSIZE - 1)];

  if (tp == NULL)
    {
      tp = table[(i ^ j) & (HASHETHSIZE - 1)] =
	(hashether_t *) g_malloc (sizeof (hashether_t));
      g_assert(tp);
    }
  else
    {
      while (1)
	{
	  if (memcmp (tp->addr, addr, sizeof (tp->addr)) == 0)
	    {
              /* addr found */
              if (!only_ethers || tp->is_name_from_file)
                return tp->name;
              else
                return NULL; /* found, but not in ethers file */
	    }
	  if (tp->next == NULL)
	    {
	      tp->next = (hashether_t *) g_malloc (sizeof (hashether_t));
              g_assert(tp->next);
	      tp = tp->next;
	      break;
	    }
	  tp = tp->next;
	}
    }

  /* fill in a new entry */
  memcpy (tp->addr, addr, sizeof (tp->addr));
  tp->next = NULL;

  eth = get_ethbyaddr (addr);
  if (!eth)
    {
      /* unknown name */
      if ((manufp = manuf_name_lookup (addr)) == NULL)
        snprintf (tp->name, MAXNAMELEN, "%s", ether_to_str ((guint8 *) addr));
      else
        snprintf (tp->name, MAXNAMELEN, "%s_%02x:%02x:%02x",
                    manufp->name, addr[3], addr[4], addr[5]);

      tp->is_name_from_file = FALSE;

    }
  else
    {
      safe_strncpy (tp->name, eth->name, MAXNAMELEN);
      tp->is_name_from_file = TRUE;
    }

  if (!only_ethers || tp->is_name_from_file)
    return tp->name;
  
  /* name not found in ethers file */
  return NULL; 

}				/* eth_name_lookup */

extern const char *
get_ether_name (const u_char * addr, gboolean only_ethers)
{
  if (!eth_resolution_initialized)
    {
      initialize_ethers ();
      eth_resolution_initialized = 1;
    }

  return eth_name_lookup (addr, only_ethers);

}				/* get_ether_name */

