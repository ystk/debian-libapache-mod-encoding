/* -*- mode: c -*-
 *
 * $Id: iconv_hook_utf8_eucjp.c,v 1.2 2002/06/10 13:24:49 tai Exp $
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

size_t ucs2_cp932(unsigned char ucs_1, unsigned char ucs_2, 
	unsigned char *cp932_1, unsigned char *cp932_2);

/*
 * utf8_eucjp_iconv_open()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static iconv_t
utf8_eucjp_iconv_open(const char *oenc, const char *ienc) {
  if(  (strncmp(ienc,"UTF-8",5) == 0)
    && ((strncmp(oenc,"EUC-JP",6) == 0) || (strncmp(oenc,"UJIS",4) == 0)
     || (strncmp(oenc,"EUCJP",5) == 0)) )
  {
  	return (iconv_t)1;
  }
  else
  {
  	return (iconv_t)-1;
  }
}

/*
 * utf8_eucjp_iconv_close() 
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
utf8_eucjp_iconv_close(iconv_t cd) {
  return 0;
}

/*
 * utf8_eucjp_iconv()
 * by KAJIKI Yoshihiro <kajiki@ylug.org>
 *  Unicode UTF-8 to EUC-JP for Microsoft Codepage 932
 */
 
static size_t
utf8_eucjp_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
  unsigned char *src, *dst;
  unsigned char ucs_1, ucs_2, cp932_1, cp932_2;
  size_t len, byte;

  if (! (srclen)) return 0;
  if (! (outlen) || outbuf == NULL || *outbuf == NULL) {
        errno=E2BIG;
        return -1;
  }
  if (srcbuf == NULL || *srcbuf == NULL) return 0;

  src = (unsigned char *)*srcbuf;
  dst = (unsigned char *)*outbuf;
  while ((*srclen > 0) && (*outlen > 0)) {
    if (((*src & 0xe0) == 0xe0) && ((*src & 0x10) == 0)) {
        byte = 3;
        ucs_1 = (*src++ & 0xf) << 4;
        ucs_1 |= ((*src & 0x3c) >> 2);
        ucs_2 = (*src++ & 0x3) << 6;
        ucs_2 |= (*src++ & 0x3f);
    } else if (((*src & 0xc0) == 0xc0) && ((*src & 0x20) == 0)) {
        byte = 2;
        ucs_1 = (*src & 0x1c) >> 2;
        ucs_2 = (*src++ & 0x30) << 2;
        ucs_2 |= (*src++ & 0x3f);
    } else if ((*src & 0x80) && ((*src & 0x40) == 0)) {
        *srcbuf=(char *)(src-1);
        errno=EILSEQ;
        return -1;
    } else {
        byte = 1;
        ucs_1 = 0;
        ucs_2 = (*src++ & 0x7f);
    }

    len = ucs2_cp932(ucs_1, ucs_2, &cp932_1, &cp932_2);
    if (len == -1) {
        *srcbuf=(char *)(src-byte);
        errno=EILSEQ;
        return -1;
    }
    *srclen-=byte;

    /* Translate Shift_JIS into EUC-JP */
    if (len == 1) {
      if (cp932_2 & 0x80) {
          *dst++ = 0x8e;
          *outlen-=1;
      }
      *dst++ = cp932_2;
      *outlen-=1;
    } else {
      cp932_1 = (cp932_1^0xa0)*2 + 0x5f;
      if (cp932_2 > 0x9e) cp932_1++;
      if (cp932_2 < 0x7f) cp932_2 += 0x61;
      else if (cp932_2 < 0x9f) cp932_2 += 0x60;
      else cp932_2 += 0x2;
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

static iconv_hook_module iconv_hook_utf8_eucjp = {
  utf8_eucjp_iconv,
  utf8_eucjp_iconv_open,
  utf8_eucjp_iconv_close,
};

iconv_hook_module *
iconv_hook_utf8_eucjp_init(void) {
  return &iconv_hook_utf8_eucjp;
}

