
#include "globals.h"
#include <glib.h>


#define MAXDNAME        1025	/* maximum domain name length */
#define NBNAME_BUF_LEN   128
#define NETBIOS_NAME_LEN  16

#define BYTES_ARE_IN_FRAME(a,b) 1
/* TODO Find a way to get capture_len here so that I can actually use this macro */


int get_dns_name (const gchar * pd, int offset, int dns_data_offset,
		  char *name, int maxname);
int process_netbios_name (const gchar * name_ptr, char *name_ret);
int ethereal_nbns_name (const gchar * pd, int offset,
			int nbns_data_offset,
			char *name_ret, int *name_type_ret);
int get_nbns_name_type_class (const gchar * pd, int offset,
			      int nbns_data_offset, char *name_ret,
			      int *name_len_ret, int *name_type_ret,
			      int *type_ret, int *class_ret);
