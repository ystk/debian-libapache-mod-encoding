/* -*- mode: c -*-
 *
 * $Id: iconv_hook_ja_auto.c,v 1.2 2002/06/08 09:32:41 tai Exp $
 * Framework Author: Taisuke Yamada (tai@iij.ad.jp)
 * Logic Composer:   Kazuhiko Iwama (iwama@ymc.ne.jp)
 */

#include <string.h>
#include "iconv_hook.h"
#include "identify_encoding.h"

/*
 * ja_auto_iconv_t
 * by Kazuhiko Iwama (iwama@ymc.ne.jp)
 * Save oenc & ienc string
 */
typedef struct {
    char* oenc;
    char* ienc;
} ja_auto_iconv_t;

/*
 * ja_auto_iconv_open()
 * by Kazuhiko Iwama (iwama@ymc.ne.jp)
 * This code is for iconv() interface compatibility.
 */
 
static iconv_t
ja_auto_iconv_open(const char *oenc, const char *ienc) {
    ja_auto_iconv_t* cd;

    if (strncasecmp("JA-AUTO", ienc, 7) != 0)  return (iconv_t)(-1);

    cd = (ja_auto_iconv_t*)malloc(sizeof(ja_auto_iconv_t));
    if (cd == NULL)  return (iconv_t)(-1);
    cd->oenc = strdup(oenc);
    cd->ienc = strdup(ienc);

    if (cd->oenc == NULL || cd->ienc == NULL){
	return (iconv_t)(-1);
    } else {
  	return (iconv_t)cd;
    }
}

/*
 * ja_auto_iconv_close() 
 * by Kazuhiko Iwama (iwama@ymc.ne.jp)
 * This code is for iconv() interface compatibility.
 * and processes nothing but returns normal return code(fixes to 1).
 */
 
static int
ja_auto_iconv_close(iconv_t _cd) {
    ja_auto_iconv_t* cd = _cd;

    if (_cd == (iconv_t)(-1) || cd == NULL) return 0;
    free(cd->oenc);
    free(cd->ienc);
    free(cd);

    return 0;
}

/* ja_auto_iconv()
 * by Kazuhiko Iwama (iwama@ymc.ne.jp)
 */
 
static size_t
ja_auto_iconv(iconv_t _cd, char **srcbuf, size_t *srclen, char **outbuf, size_t *outlen) {
    iconv_hook_module *cm;
    ja_auto_iconv_t   *cd = _cd;
    iconv_t            cd2;
    size_t             ret = -1;
    const char        *enc;

    if (! (srcbuf && srclen && outbuf && outlen))
      return 0;

    enc = autodetect_encoding(*srcbuf, cd->ienc);
    if (strcasecmp(enc, "MSSJIS") == 0){
	if (strcasecmp(cd->oenc, "UTF-8") == 0 || strcasecmp(cd->oenc, "UTF8") == 0){
	    cm = iconv_hook_mssjis_init();
	} else {
	    cm = iconv_hook_default_init();
	    enc = "SJIS";
	}
    } else {
        cm = iconv_hook_default_init();
    }

    if ( cm != NULL && (cd2 = (*(cm->iconv_open))(cd->oenc, enc)) != (iconv_t)(-1)) {
	ret = (*(cm->iconv))(cd2, srcbuf, srclen, outbuf, outlen);
	(*(cm->iconv_close))(cd2);
    }

    return ret;
}

static iconv_hook_module iconv_hook_ja_auto = {
    ja_auto_iconv,
    ja_auto_iconv_open,
    ja_auto_iconv_close,
};

iconv_hook_module *
iconv_hook_ja_auto_init(void) {
    return &iconv_hook_ja_auto;
}
