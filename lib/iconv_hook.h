/* -*- mode: c -*-
 *
 * $Id: iconv_hook.h,v 1.4 2002/06/10 13:57:52 tai Exp $
 *
 */

#ifndef ICONV_HOOK_H
#define ICONV_HOOK_H

#ifdef HAVE_ICONV_H
#include <iconv.h>
#else
typedef void * iconv_t;
#endif

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef EILSEQ /* Not always defined on all system */
#define EILSEQ -2323
#endif

/*
 * module structure for encoding converter module
 */
typedef struct iconv_hook_module_struct {
  size_t  (*iconv)(iconv_t cd,
		   char **src, size_t *srclen, char **outbuf, size_t *outlen);
  iconv_t (*iconv_open) (const char *ienc, const char *oenc);
  int     (*iconv_close)(iconv_t cd);
} iconv_hook_module;

/* EDITME - declaration for custom module initializer(s) */
extern iconv_hook_module * iconv_hook_ja_auto_init(void);
extern iconv_hook_module * iconv_hook_mssjis_init(void);
extern iconv_hook_module * iconv_hook_eucjp_init(void);
extern iconv_hook_module * iconv_hook_ucs2_cp932_init(void);
extern iconv_hook_module * iconv_hook_utf8_cp932_init(void);
extern iconv_hook_module * iconv_hook_utf8_eucjp_init(void);
extern iconv_hook_module * iconv_hook_default_init(void);

/* EDITME - register above initializer(s) to lookup table */
static iconv_hook_module * (*iconv_hook_module_init[])(void) = {
  iconv_hook_ja_auto_init,
  iconv_hook_mssjis_init,
  iconv_hook_eucjp_init,
  iconv_hook_ucs2_cp932_init,
  iconv_hook_utf8_cp932_init,
  iconv_hook_utf8_eucjp_init,
  iconv_hook_default_init,
  NULL,
};

#endif
