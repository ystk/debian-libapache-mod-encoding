/* -*- mode: c -*-
 *
 * $Id: iconv_hook.c,v 1.2 2002/06/08 09:19:01 tai Exp $
 *
 */

#include "iconv_hook.h"

typedef struct {
  iconv_hook_module *cm;
  iconv_t            cd;
} iconv_hook_t;

iconv_t
iconv_hook_open(const char *ienc, const char *oenc) {
  iconv_hook_t *p;
  int i;

  if ((p = (iconv_hook_t *)malloc(sizeof(iconv_hook_t))) == NULL) {
    return (iconv_t)(-1);
  }

  for (i = 0 ; iconv_hook_module_init[i] ; i++) {
    if ((p->cm = (*iconv_hook_module_init[i])()) &&
	(p->cd = (*(p->cm->iconv_open))(ienc, oenc)) != (iconv_t)(-1)) {
      return (iconv_t)p;
    }
  }
  free(p);

  return (iconv_t)(-1);
}

int
iconv_hook_close(iconv_t cd) {
  free((iconv_hook_t *)cd);
  return 0;
}

size_t
iconv_hook(iconv_t cd,
	   char **src, size_t *srclen, char **outbuf, size_t *outlen) {
  iconv_hook_t *p = (iconv_hook_t *)cd;

  return (*(p->cm->iconv))(p->cd, src, srclen, outbuf, outlen);
}
