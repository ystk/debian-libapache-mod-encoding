/* -*- mode: c -*-
 *
 * $Id: iconv_hook_default.c,v 1.3 2002/06/08 09:19:01 tai Exp $
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "iconv_hook.h"

/**
 * Default plug-in for mod_encoding.
 * This also exists to show how custom converter can be plugged in.
 *
 * All plug-ins must have at least 5 elements to work:
 * 3 iconv_*(3) compatible functions, 1 struct to bundle previous
 * 3 functions, and 1 initializer to return handle to the structure.
 *
 * As this plug-in is only a wrapper to real iconv_*(3), there
 * is almost no complexity. This should be a good starting point
 * if you are trying to write your own custom converter plug-in.
 *
 * @author  Taisuke Yamada <tai@iij.ad.jp>
 * @version $Revision: 1.3 $
 */

/**
 * iconv_open(3) compatible function
 */
static iconv_t
local_iconv_open(const char *ienc, const char *oenc) {
#ifdef WITH_ICONV
  return iconv_open(ienc, oenc);
#else
  return (iconv_t)-1;
#endif
}

/**
 * iconv_close(3) compatible function
 */
static int
local_iconv_close(iconv_t cd) {
#ifdef WITH_ICONV
  return iconv_close(cd);
#else
  return 0;
#endif
}

/**
 * iconv(3) compatible function
 */
static size_t
local_iconv(iconv_t cd,
	    char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
#ifdef WITH_ICONV
#if __GLIBC__ == 2 && __GLIBC_MINOR__ == 1
  /* iconv in glibc-2.1 is known to break with NULL paramater */
  if (! (srcbuf && srclen && outbuf && outlen))
    return 0;
#endif
  return iconv(cd, srcbuf, srclen, outbuf, outlen);
#else
  return 0;
#endif
}

/**
 * plug-in strcture - see iconv_hook.h
 */
static iconv_hook_module iconv_hook_default = {
  local_iconv,
  local_iconv_open,
  local_iconv_close,
};

/**
 * initializer
 */
iconv_hook_module *
iconv_hook_default_init(void) {
  return &iconv_hook_default;
}
