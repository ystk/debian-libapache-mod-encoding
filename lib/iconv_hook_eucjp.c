/* -*- mode: c -*-
 *
 * $Id: iconv_hook_eucjp.c,v 1.4 2002/06/10 13:57:52 tai Exp $
 * Author: KAJIKI Yoshihiro <kajiki@ylug.org>
 * based on the 'iconv_hook_mssjis.c' by Kunio Miyamoto (wakatono@todo.gr.jp)
 */

#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include "iconv_hook.h"

#define	SS2		(0x8E)
#define	SS3		(0x8F)
#define is_ascii(c) \
        ((unsigned char) (c) < 0x80)	/* ISO 646 */
#define is_kanji(c) \
        ((unsigned char) (c) > 0x9F)	/* JIS X 0208 */
#define is_hankana(c) \
        ((unsigned char) (c) == SS2)	/* JIS X 0201 */
#define is_hojyo(c) \
        ((unsigned char) (c) == SS3)	/* JIS X 0212 */

size_t mssjis_iconv(iconv_t cd,
            char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen);

static size_t skip_bytes(char c)
{
  if (is_ascii(c)) {
    return 1;
  } else if (is_kanji(c) || is_hankana(c)) {
    return 2;
  } else if (is_hojyo(c)) {
    return 3;
  }
  return 0;
}

/*
 * eucjp_iconv_open()
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static iconv_t
eucjp_iconv_open(const char *oenc, const char *ienc) {
  if( ((strncmp(ienc,"EUC-JP",6) == 0) || (strncmp(ienc,"UJIS",4) == 0)
     || (strncmp(ienc,"EUCJP",5) == 0)) && (strncmp(oenc,"UTF-8",5) == 0) )
  {
  	return (iconv_t)1;
  }
  else
  {
  	return (iconv_t)-1;
  }
}

/*
 * eucjp_iconv_close() 
 * by Kunio Miyamoto (wakatono@todo.gr.jp)
 * and KAJIKI Yoshihiro <kajiki@ylug.org>
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
eucjp_iconv_close(iconv_t cd) {
  return 0;
}

/* eucjp_iconv()
 * by KAJIKI Yoshihiro <kajiki@ylug.org>
 *  EUC-JP code to UTF-8 via Microsoft ShiftJIS using mssjis_iconv().
 * This is experimental code for processing EUC-JP.
 */
 
static size_t
eucjp_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
  unsigned char *tmpbuf, *tmp;
  unsigned char *src;
  unsigned char ch, cl;
  size_t ret;

  if (! (srcbuf && srclen && outbuf && outlen))
    return 0;

  /* translate EUC-JP into SJIS */
  src = (unsigned char *)*srcbuf;
  tmp = tmpbuf = malloc(*srclen+2);
  while (*src && ((tmp - tmpbuf) < *srclen)) {
    ch = *src++;
    if (is_ascii(ch)) {
      *tmp++ = ch;
    } else {
      cl = *src++;
      if (is_kanji(ch)) {
        *tmp++ = ((ch-0x5f)/2) ^ 0xA0;
        if (!(ch&1))
          *tmp++ = cl - 0x02;
        else if (cl < 0xE0)
          *tmp++ = cl - 0x61;
        else
          *tmp++ = cl - 0x60;
      } else if (is_hankana(ch)) {
        if (cl < 0xA0 || cl > 0xDF) {
          *srcbuf=(char *)(src-2);
          errno=EILSEQ;
          return -1;
        }
        *tmp++ = cl;
      } else {
        /* We don't support JIS X 0212 */
        *srcbuf=(char *)(src-2);
        errno=EILSEQ;
        return -1;
      }
    }
  }
  *tmp='\0';

  ret = mssjis_iconv(cd, (char **) &tmpbuf, srclen, outbuf, outlen);
  free(tmpbuf);
  *srcbuf += *src;
  *srclen = 0;
  return ret;
}

static iconv_hook_module iconv_hook_eucjp = {
  eucjp_iconv,
  eucjp_iconv_open,
  eucjp_iconv_close,
};

iconv_hook_module *
iconv_hook_eucjp_init(void) {
  return &iconv_hook_eucjp;
}

