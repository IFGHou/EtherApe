
#include "names_netbios.h"

int
get_dns_name (const gchar * pd, int offset, int dns_data_offset,
	      char *name, int maxname)
{
  const gchar *dp = pd + offset;
  const gchar *dptr = dp;
  char *np = name;
  int len = -1;
  u_int component_len;

  maxname--;			/* reserve space for the trailing '\0' */
  for (;;)
    {
      if (!BYTES_ARE_IN_FRAME (offset, 1))
	goto overflow;
      component_len = *dp++;
      offset++;
      if (component_len == 0)
	break;
      switch (component_len & 0xc0)
	{

	case 0x00:
	  /* Label */
	  if (np != name)
	    {
	      /* Not the first component - put in a '.'. */
	      if (maxname > 0)
		{
		  *np++ = '.';
		  maxname--;
		}
	    }
	  if (!BYTES_ARE_IN_FRAME (offset, component_len))
	    goto overflow;
	  while (component_len > 0)
	    {
	      if (maxname > 0)
		{
		  *np++ = *dp;
		  maxname--;
		}
	      component_len--;
	      dp++;
	      offset++;
	    }
	  break;

	case 0x40:
	case 0x80:
	  goto error;		/* error */

	case 0xc0:
	  /* Pointer. */
	  /* XXX - check to make sure we aren't looping, by keeping track
	     of how many characters are in the DNS packet, and of how many
	     characters we've looked at, and quitting if the latter
	     becomes bigger than the former. */
	  if (!BYTES_ARE_IN_FRAME (offset, 1))
	    goto overflow;
	  offset =
	    dns_data_offset + (((component_len & ~0xc0) << 8) | (*dp++));
	  /* If "len" is negative, we are still working on the original name,
	     not something pointed to by a pointer, and so we should set "len"
	     to the length of the original name. */
	  if (len < 0)
	    len = dp - dptr;
	  dp = pd + offset;
	  break;		/* now continue processing from there */
	}
    }

error:
  *np = '\0';
  /* If "len" is negative, we haven't seen a pointer, and thus haven't
     set the length, so set it. */
  if (len < 0)
    len = dp - dptr;
  /* Zero-length name means "root server" */
  if (*name == '\0')
    strcpy (name, "<Root>");
  return len;

overflow:
  /* We ran past the end of the captured data in the packet. */
  strcpy (name, "<Name goes past end of captured data in packet>");
  /* If "len" is negative, we haven't seen a pointer, and thus haven't
     set the length, so set it. */
  if (len < 0)
    len = dp - dptr;
  return len;
}

int
process_netbios_name (const gchar * name_ptr, char *name_ret)
{
  int i;
  int name_type = *(name_ptr + NETBIOS_NAME_LEN - 1);
  gchar name_char;
  static const char hex_digits[16] = "0123456780abcdef";

  for (i = 0; i < NETBIOS_NAME_LEN - 1; i++)
    {
      name_char = *name_ptr++;
      if (name_char >= ' ' && name_char <= '~')
	*name_ret++ = name_char;
      else
	{
	  /* It's not printable; show it as <XX>, where
	     XX is the value in hex. */
	  *name_ret++ = '<';
	  *name_ret++ = hex_digits[(name_char >> 4)];
	  *name_ret++ = hex_digits[(name_char & 0x0F)];
	  *name_ret++ = '>';
	}
    }
  *name_ret = '\0';
  return name_type;
}

int
ethereal_nbns_name (const gchar * pd, int offset, int nbns_data_offset,
		    char *name_ret, int *name_type_ret)
{
  int name_len;
  char name[MAXDNAME];
  char nbname[NBNAME_BUF_LEN];
  char *pname, *pnbname, cname, cnbname;
  int name_type;

  name_len = get_dns_name (pd, offset, nbns_data_offset, name, sizeof (name));

  /* OK, now undo the first-level encoding. */
  pname = &name[0];
  pnbname = &nbname[0];
  for (;;)
    {
      /* Every two characters of the first level-encoded name
       * turn into one character in the decoded name. */
      cname = *pname;
      if (cname == '\0')
	break;			/* no more characters */
      if (cname == '.')
	break;			/* scope ID follows */
      if (cname < 'A' || cname > 'Z')
	{
	  /* Not legal. */
	  strcpy (nbname,
		  "Illegal NetBIOS name (character not between A and Z in first-level encoding)");
	  goto bad;
	}
      cname -= 'A';
      cnbname = cname << 4;
      pname++;

      cname = *pname;
      if (cname == '\0' || cname == '.')
	{
	  /* No more characters in the name - but we're in
	   * the middle of a pair.  Not legal. */
	  strcpy (nbname, "Illegal NetBIOS name (odd number of bytes)");
	  goto bad;
	}
      if (cname < 'A' || cname > 'Z')
	{
	  /* Not legal. */
	  strcpy (nbname,
		  "Illegal NetBIOS name (character not between A and Z in first-level encoding)");
	  goto bad;
	}
      cname -= 'A';
      cnbname |= cname;
      pname++;

      /* Do we have room to store the character? */
      if (pnbname < &nbname[NETBIOS_NAME_LEN])
	{
	  /* Yes - store the character. */
	  *pnbname = cnbname;
	}

      /* We bump the pointer even if it's past the end of the
         name, so we keep track of how long the name is. */
      pnbname++;
    }

  /* NetBIOS names are supposed to be exactly 16 bytes long. */
  if (pnbname - nbname != NETBIOS_NAME_LEN)
    {
      /* It's not. */
      sprintf (nbname, "Illegal NetBIOS name (%ld bytes long)",
	       (long) (pnbname - nbname));
      goto bad;
    }

  /* This one is; make its name printable. */
  name_type = process_netbios_name (nbname, name_ret);
  name_ret += strlen (name_ret);
  sprintf (name_ret, "<%02x>", name_type);
  name_ret += 4;
  if (cname == '.')
    {
      /* We have a scope ID, starting at "pname"; append that to
       * the decoded host name. */
      strcpy (name_ret, pname);
    }
  if (name_type_ret != NULL)
    *name_type_ret = name_type;
  return name_len;

bad:
  if (name_type_ret != NULL)
    *name_type_ret = -1;
  strcpy (name_ret, nbname);
  return name_len;
}


int
get_nbns_name_type_class (const gchar * pd, int offset, int nbns_data_offset,
			  char *name_ret, int *name_len_ret,
			  int *name_type_ret, int *type_ret, int *class_ret)
{
  int name_len;
  int type;
  int class;

  name_len = ethereal_nbns_name (pd, offset, nbns_data_offset, name_ret,
				 name_type_ret);
  offset += name_len;

#if 0
/* I'm leaving this out at least until I pass captured_len to this function */
  if (!BYTES_ARE_IN_FRAME (offset, 2))
    {
      /* We ran past the end of the captured data in the packet. */
      return -1;
    }
#endif
  type = pntohs (&pd[offset]);
  offset += 2;

#if 0
  if (!BYTES_ARE_IN_FRAME (offset, 2))
    {
      /* We ran past the end of the captured data in the packet. */
      return -1;
    }
#endif
  class = pntohs (&pd[offset]);

  *type_ret = type;
  *class_ret = class;
  *name_len_ret = name_len;

  return name_len + 4;
}
