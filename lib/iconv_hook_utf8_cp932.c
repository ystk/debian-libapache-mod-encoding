/* -*- mode: c -*-
 *
 * $Id: iconv_hook_utf8_cp932.c,v 1.2 2002/06/10 13:24:49 tai Exp $
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
 * utf8_cp932_iconv_open()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static iconv_t
utf8_cp932_iconv_open(const char *oenc, const char *ienc) {
  if(  (strncmp(ienc,"UTF-8",5) == 0)
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
 * utf8_cp932_iconv_close() 
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
utf8_cp932_iconv_close(iconv_t cd) {
  return 0;
}

/*
 * utf8_cp932_iconv()
 * by KAJIKI Yoshihiro <kajiki@ylug.org>
 *  Unicode UTF-8 to Microsoft Codepage 932 (Shift_JIS)
 */
 
static size_t
utf8_cp932_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
  unsigned char *src, *dst;
  unsigned char ucs_1, ucs_2, cp932_1, cp932_2;
  size_t len, byte;

  if (! (srcbuf && srclen && outbuf && outlen))
    return 0;

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

static iconv_hook_module iconv_hook_utf8_cp932 = {
  utf8_cp932_iconv,
  utf8_cp932_iconv_open,
  utf8_cp932_iconv_close,
};

iconv_hook_module *
iconv_hook_utf8_cp932_init(void) {
  return &iconv_hook_utf8_cp932;
}

