/* -*- mode: c -*-
 *
 * $Id: iconv_hook_ucs2_cp932.c,v 1.2 2002/06/10 13:24:49 tai Exp $
 * Framework Author: Taisuke Yamada (tai@iij.ad.jp)
 * Logic Composer:   Kunio Miyamoto (wakatono@todo.gr.jp)
 * Author: KAJIKI Yoshihiro <kajiki@ylug.org>
 * based on the 'iconv_hook_mssjis.c' by Kunio Miyamoto (wakatono@todo.gr.jp)
 */

#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "iconv_hook.h"
#include "ucs2_cp932.h"

/*
 * ucs2_cp932_iconv_open()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static iconv_t
ucs2_cp932_iconv_open(const char *oenc, const char *ienc) {
  if(  ((strncmp(ienc,"UCS-2",5) == 0) || (strncmp(ienc,"UNICODE",7) == 0))
    && ((strncmp(oenc,"CP932",5) == 0) || (strncmp(oenc,"SHIFT-JIS",9) == 0)
     || (strncmp(oenc,"SHIFT_JIS",9) == 0) || (strncmp(oenc,"SJIS",4) == 0)) )
  {
  	return (iconv_t)1;
  }
  else
  {
  	return (iconv_t)-1;
  }
}

/*
 * ucs2_cp932_iconv_close() 
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
ucs2_cp932_iconv_close(iconv_t cd) {
  return 0;
}

/*
 * This is experimental code for processing Microsoft Shift_JIS Code
 * This routine uses fixed table_ucs2_cp932[] in "ucs2_cp932.h", 
 * and does'nt be needed to load conversion table.
 */
size_t ucs2_cp932(unsigned char ucs_1, unsigned char ucs_2, 
	unsigned char *cp932_1, unsigned char *cp932_2) {
  unsigned char *table;

  table = table_ucs2_cp932[ucs_1];
  if (table == NULL) {
    return -1;
  }
  *cp932_1 = table[(int)ucs_2*2];
  *cp932_2 = table[(int)ucs_2*2+1];

  if (*cp932_1 == 0xff) {
    if (*cp932_2 == 0xff) return -1;
    else return 1;
  } else return 2;
}

/*
 * ucs2_cp932_iconv()
 * by KAJIKI Yoshihiro <kajiki@ylug.org>
 *  Unicode UCS-2 to Microsoft Codepage 932 (Shift_JIS)
 */
 
static size_t
ucs2_cp932_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
  unsigned char *src, *dst;
  unsigned char ucs_1, ucs_2, cp932_1, cp932_2;
  size_t len;

  if (! (srclen)) return 0;
  if (! (outlen) || outbuf == NULL || *outbuf == NULL) {
        errno=E2BIG;
        return -1;
  }
  if (srcbuf == NULL || *srcbuf == NULL) return 0;

  src = (unsigned char *)*srcbuf;
  dst = (unsigned char *)*outbuf;
  while (*srclen && *outlen) {
    ucs_1 = *src++;
    ucs_2 = *src++;
    *srclen-=2;
    len = ucs2_cp932(ucs_1, ucs_2, &cp932_1, &cp932_2);
    if (len == -1) {
        *srcbuf=(char *)(src-2);
        errno=EILSEQ;
        return -1;
    }

    if (len == 1) {
      *dst++ = cp932_2;
      *outlen-=1;
    } else {
      *dst++ = cp932_1;
      *dst++ = cp932_2;
      *outlen-=2;
    }
  }
  *srcbuf=(char *)src;
  *outbuf=(char *)dst;
  if (! (*outlen)) *dst++='\0';
  if (! (*srclen)) return 0;
  errno=E2BIG;
  return -1;
}

static iconv_hook_module iconv_hook_ucs2_cp932 = {
  ucs2_cp932_iconv,
  ucs2_cp932_iconv_open,
  ucs2_cp932_iconv_close,
};

iconv_hook_module *
iconv_hook_ucs2_cp932_init(void) {
  return &iconv_hook_ucs2_cp932;
}

