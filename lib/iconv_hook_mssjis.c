/* -*- mode: c -*-
 *
 * $Id: iconv_hook_mssjis.c,v 1.3 2002/06/10 13:24:49 tai Exp $
 * Framework Author: Taisuke Yamada (tai@iij.ad.jp)
 * Logic Composer:   Kunio Miyamoto (wakatono@todo.gr.jp)
 */

#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#include "iconv_hook.h"
#include "cp.h"

#define	MSSJISENC	"MSSJIS"
#define is_zen(c) \
        ((0x81 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0x9f) \
        || (0xe0 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xfc))
#define is_han(c) \
        ((0xa0 <= ((unsigned char) (c)) && ((unsigned char) (c)) <= 0xdf))

static size_t skip_bytes(char c)
{
  if(is_zen(c)) {
    return 2;
  } else if (is_han(c)) {
    return 1;
  }
  return 0;
}

/*
 * mssjis_iconv_open()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static iconv_t
mssjis_iconv_open(const char *oenc, const char *ienc) {
  if( (strncmp(ienc,MSSJISENC,6) == 0) && (strncmp(oenc,"UTF-8",5) == 0) )
  {
  	return (iconv_t)1;
  }
  else
  {
  	return (iconv_t)-1;
  }
}

/*
 * mssjis_iconv_close() 
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
mssjis_iconv_close(iconv_t cd) {
  return 0;
}

/* mssjis_iconv()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 *  Microsoft ShiftJIS code to UTF-8 
 * This is experimental code for processing Microsoft Shift JIS Code :-(
 * This routine uses fixed table cp[] in "cp.h" , and does'nt be needed to
 * load conversion table.
 */
 
size_t
mssjis_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
  unsigned char *dst;
  unsigned char *src;
  unsigned short utf8code;
  int sjiscode;
  size_t len;
  unsigned char *to2;

  if (! (srcbuf && srclen && outbuf && outlen))
    return 0;

  src = (unsigned char *)*srcbuf;
  dst = to2 = malloc(*outlen);
  while (*src && ((dst - to2) < (*outlen - 4))) {
    len = skip_bytes(*src);
    if ( len == 2 ) {
      sjiscode = (int)(*src++ & 0xff);
      sjiscode = (int)((sjiscode << 8)|(*src++ & 0xff));
    } else {
      sjiscode = (int)(*src++ & 0xff);
    }
    utf8code = cp[sjiscode]; /* convert sjis code to utf8 (cp[] is conversion table array) */

    if ( utf8code <= 0x7f ) {
      *dst++ = (char)(utf8code & 0xff);
    } else if ( utf8code <= 0x7ff ){
      *dst++ = (char)( 0xc0 | ((utf8code >> 6) & 0xff));
      *dst++ = (char)( 0x80 | ( utf8code & 0x3f ));
    } else {
      *dst++ = (char)( 0xe0 | ((utf8code >> 12) & 0x0f));
      *dst++ = (char)( 0x80 | ((utf8code >> 6)  & 0x3f));
      *dst++ = (char)( 0x80 | (utf8code & 0x3f));
    }

  }
  *dst++='\0';
  memcpy(*outbuf,to2,*outlen);
  free(to2);
  *srcbuf += *src;
  *srclen = 0;
  *outbuf = dst;
  *outlen = strlen(*outbuf);
  return strlen(*outbuf);
}

static iconv_hook_module iconv_hook_mssjis = {
  mssjis_iconv,
  mssjis_iconv_open,
  mssjis_iconv_close,
};

iconv_hook_module *
iconv_hook_mssjis_init(void) {
  return &iconv_hook_mssjis;
}

