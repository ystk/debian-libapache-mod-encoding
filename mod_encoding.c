/* -*- mode: c -*-
 *
 * $Id: mod_encoding.c,v 1.10 2002/06/11 09:07:14 tai Exp $
 *
 */

#include <httpd.h>
#include <http_config.h>
#include <http_core.h>
#include <http_log.h>
#include <http_protocol.h>
#include <http_request.h>

#include <iconv.h>

/**
 * Core part of the module.
 * Here, the module hooks into filename translation stage,
 * and converts all non-ascii-or-utf8 expressions into
 * appropriate encoding speficied by the configuration.
 *
 * Note UTF-8 is the implicit default and will be tried first,
 * regardless of user configuration.
 */

#ifndef MOD_ENCODING_DEBUG
#ifdef DEBUG
#define MOD_ENCODING_DEBUG 1
#else
#define MOD_ENCODING_DEBUG 0
#endif
#endif

#define DBG(expr) if (MOD_ENCODING_DEBUG) { expr; }

#ifdef __GNUC__
#define LOG(level, server, args...) \
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|level, server, ##args)
#else
#define LOG(level, server, ...) \
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|level, server, __VA_ARGS__)
#endif

#define ENABLE_FLAG_UNSET 0
#define ENABLE_FLAG_OFF   1
#define ENABLE_FLAG_ON    2

#define STRIP_FLAG_UNSET  0
#define STRIP_FLAG_OFF    1
#define STRIP_FLAG_ON     2

/**
 * module-local information storage structure
 */
typedef struct {
  int           enable_function;  /* flag to enable this module */
  char         *server_encoding;  /* server-side filesystem encoding */
  array_header *client_encoding;  /* useragent-to-encoding-list sets */
  array_header *default_encoding; /* useragent-to-encoding-list sets */

  int           strip_msaccount;  /* normalize wierd WinXP username */
} encoding_config;

module MODULE_VAR_EXPORT encoding_module;

/***************************************************************************
 * utility methods
 ***************************************************************************/

/**
 * Converts encoding of the input string.
 * Returns NULL on error, else, appropriate string on success.
 *
 * @param p      Memory pool of apache
 * @param cd     Conversion descriptor, made by iconv_open(3).
 * @param srcbuf Input string
 * @param srclen Length of the input string. Usually strlen(srcbuf).
 */
static char *
iconv_string(request_rec *r, iconv_t cd, char *srcbuf, size_t srclen) {

  char   *outbuf, *marker;
  size_t  outlen;

  if (srclen == 0) {
    LOG(APLOG_DEBUG, r->server, "iconv_string: skipping zero-length input");
    return srcbuf;
  }

  /* Allocate space for conversion. Note max bloat factor is 4 of UCS-4 */
  marker = outbuf = (char *)ap_palloc(r->pool, outlen = srclen * 4 + 1);

  if (outbuf == NULL) {
    LOG(APLOG_WARNING, r->server, "iconv_string: no more memory");
    return NULL;
  }

  /* Convert every character within input string. */
  while (srclen > 0) {
    if (iconv(cd, &srcbuf, &srclen, &outbuf, &outlen) == (size_t)(-1)) {
      LOG(APLOG_DEBUG, r->server, "iconv_string: conversion error");
      return NULL;
    }
  }

#if 0 /* Commented out for now as some iconv seems to break with NULL */
  /* Everything done. Flush buffer/state and return result */
  iconv(cd, NULL, NULL, &outbuf, &outlen);
  iconv(cd, NULL, NULL, NULL, NULL);
#endif

  *outbuf = '\0';

  return marker;
}

/**
 * Nomalize charset in HTTP request line and HTTP header(s).
 * Returns 0 on success, -1 (non-zero) on error.
 *
 * FIXME: Should handle/consider partial success?
 *
 * @param  r Apache request object structure
 * @param cd Conversion descriptor, made by iconv_open(3).
 */
static int
iconv_header(request_rec *r, iconv_t cd) {

  char *buff;
  char *keys[] = { "Destination", NULL };
  int   i;

  /* Normalize encoding in HTTP request line */
  ap_unescape_url(r->unparsed_uri);
  if ((buff = iconv_string(r, cd, r->unparsed_uri,
			   strlen(r->unparsed_uri))) == NULL)
    return -1;
  ap_parse_uri(r, buff);
  ap_getparents(r->uri); /* normalize given path for security */

  /* Normalize encoding in HTTP request header(s) */
  for (i = 0 ; keys[i] ; i++) {
    if ((buff = (char *)ap_table_get(r->headers_in, keys[i])) != NULL) {
      ap_unescape_url(buff);
      if ((buff = iconv_string(r, cd, buff, strlen(buff))) == NULL)
	return -1;
      ap_table_set(r->headers_in, keys[i], buff);
    }
  }

  return 0;
}

/**
 * Return the list of encoding(s) (defaults to (list "UTF-8"))
 * which named client is expected to send.
 *
 * @param r      Apache request object structure
 * @param encmap Table of UA-to-encoding(s)
 * @param lookup Name of the useragent to look for
 */
static array_header *
get_client_encoding(request_rec *r,
		    array_header *encmap, const char *lookup) {
  void         **list = (void **)encmap->elts;
  array_header  *encs = ap_make_array(r->pool, 1, sizeof(char *));

  int i;

  LOG(APLOG_DEBUG, r->server, "get_client_encoding: entered");

  /* push UTF-8 as the first candidate of expected encoding */
  *((char **)ap_push_array(encs)) = ap_pstrdup(r->pool, "UTF-8");

  if (! lookup)
    return encs;

  LOG(APLOG_DEBUG, r->server, "get_client_encoding: lookup == %s", lookup);

  for (i = 0 ; i < encmap->nelts ; i += 2) {
    if (ap_regexec((regex_t *)list[i], lookup, 0, NULL, 0) == 0) {
      LOG(APLOG_DEBUG, r->server, "get_client_encoding: entry found");
      ap_array_cat(encs, (array_header *)list[i + 1]);
      return encs;
    }
  }

  LOG(APLOG_DEBUG, r->server, "get_client_encoding: entry not found");
  return encs;
}

/**
 * Handler for "EncodingEngine" directive.
 */
static const char *
set_encoding_engine(cmd_parms *cmd, encoding_config *conf, int flag) {
  LOG(APLOG_DEBUG, cmd->server, "set_encoding_engine: entered");
  LOG(APLOG_DEBUG, cmd->server, "set_encoding_engine: flag == %d", flag);

  if (! cmd->path) {
    conf = ap_get_module_config(cmd->server->module_config, &encoding_module);
  }
  conf->enable_function = flag ? ENABLE_FLAG_ON : ENABLE_FLAG_OFF;

  return NULL;
}

/**
 * Handler for "SetServerEncoding" directive.
 */
static const char *
set_server_encoding(cmd_parms *cmd, encoding_config *conf, char *arg) {
  LOG(APLOG_DEBUG, cmd->server, "set_server_encoding: entered");
  LOG(APLOG_DEBUG, cmd->server, "set_server_encoding: arg == %s", arg);

  if (! cmd->path) {
    conf = ap_get_module_config(cmd->server->module_config, &encoding_module);
  }
  conf->server_encoding = ap_pstrdup(cmd->pool, arg);
  
  return NULL;
}

/**
 * Handler for "AddClientEncoding" directive.
 *
 * This registers regex pattern of UserAgent: header and expected
 * encoding(s) from that useragent.
 */
static const char *
add_client_encoding(cmd_parms *cmd, encoding_config *conf, char *args) {
  array_header    *encs;
  char            *arg;

  LOG(APLOG_DEBUG, cmd->server, "add_client_encoding: entered");
  LOG(APLOG_DEBUG, cmd->server, "add_client_encoding: args == %s", args);

  if (! cmd->path) {
    conf = ap_get_module_config(cmd->server->module_config, &encoding_module);
  }

  encs = ap_make_array(cmd->pool, 1, sizeof(void *));

  /* register useragent with UserAgent: pattern */
  if (*args && (arg = ap_getword_conf_nc(cmd->pool, &args))) {
    LOG(APLOG_DEBUG, cmd->server, "add_client_encoding: agent: %s", arg);
    *(void **)ap_push_array(conf->client_encoding) =
      ap_pregcomp(cmd->pool, arg, REG_EXTENDED|REG_ICASE|REG_NOSUB);
  }

  /* register list of possible encodings from above useragent */
  while (*args && (arg = ap_getword_conf_nc(cmd->pool, &args))) {
    LOG(APLOG_DEBUG, cmd->server, "add_client_encoding: encname: %s", arg);
    *(void **)ap_push_array(encs) = ap_pstrdup(cmd->pool, arg);
  }
  *(void **)ap_push_array(conf->client_encoding) = encs;

  return NULL;
}

/**
 * Handler for "DefaultClientEncoding" directive.
 *
 * This registers encodings that work as "fallback" for most clients.
 */
static const char *
default_client_encoding(cmd_parms *cmd, encoding_config *conf, char *args) {
  char *arg;

  LOG(APLOG_DEBUG, cmd->server, "default_client_encoding: entered");
  LOG(APLOG_DEBUG, cmd->server, "default_client_encoding: args == %s", args);

  if (! cmd->path) {
    conf = ap_get_module_config(cmd->server->module_config, &encoding_module);
  }

  conf->default_encoding = ap_make_array(cmd->pool, 1, sizeof(char *));

  /* register list of possible encodings as a default */
  while (*args && (arg = ap_getword_conf_nc(cmd->pool, &args))) {
    LOG(APLOG_DEBUG, cmd->server, "default_client_encoding: encname: %s", arg);
    *(void **)ap_push_array(conf->default_encoding)
      = ap_pstrdup(cmd->pool, arg);
  }

  return NULL;
}

/**
 * Handler for "NormalizeUsername" directive.
 *
 * NOTE: This has nothing to do with encoding conversion.
 *       So where should this go?
 */
static const char *
set_normalize_username(cmd_parms *cmd, encoding_config *conf, int flag) {
  LOG(APLOG_DEBUG, cmd->server, "set_normalize_username: entered");
  LOG(APLOG_DEBUG, cmd->server, "set_normalize_username: flag == %d", flag);

  if (! cmd->path) {
    conf = ap_get_module_config(cmd->server->module_config, &encoding_module);
  }
  conf->strip_msaccount = flag ? STRIP_FLAG_ON : STRIP_FLAG_OFF;

  return NULL;
}

/***************************************************************************
 * module-unique command table
 *
 * FIXME: decide which scope to apply for following directives
 ***************************************************************************/

static const command_rec mod_enc_commands[] = {
  {"EncodingEngine",
   set_encoding_engine, NULL,
   OR_ALL, FLAG,  "Usage: EncodingEngine (on|off)"},

  {"SetServerEncoding",
   set_server_encoding, NULL,
   OR_ALL, TAKE1, "Usage: SetServerEncoding <enc>"},

  {"AddClientEncoding",
   add_client_encoding, NULL,
   OR_ALL, RAW_ARGS, "Usage: AddClientEncoding <agent> <enc> [<enc> ...]"},

  {"DefaultClientEncoding",
   default_client_encoding, NULL,
   OR_ALL, RAW_ARGS, "Usage: DefaultClientEncoding <enclist>"},

  {"NormalizeUsername",
   set_normalize_username, NULL,
   OR_ALL, FLAG, "Usage: NormalizeUsername (on|off)"},

  {NULL}
};

/***************************************************************************
 * module methods
 ***************************************************************************/

/**
 * Setup server-level module internal data strcuture.
 */
static void *
server_setup(pool *p, server_rec *s) {
  encoding_config *conf;

  DBG(fprintf(stderr, "server_setup: entered\n"));

  conf = (encoding_config *)ap_pcalloc(p, sizeof(encoding_config));
  conf->enable_function  = ENABLE_FLAG_UNSET;
  conf->server_encoding  = NULL;
  conf->client_encoding  = ap_make_array(p, 2, sizeof(void *));
  conf->default_encoding = NULL;
  conf->strip_msaccount  = STRIP_FLAG_UNSET;

  return conf;
}

/**
 * Setup folder-level module internal data strcuture.
 */
static void *
folder_setup(pool *p, char *dir) {
  DBG(fprintf(stderr, "folder_setup: entered\n"));
  return server_setup(p, NULL);
}

/**
 * Merge configuration.
 */
static void *
config_merge(pool *p, void *base, void *override) {
  encoding_config *parent = base;
  encoding_config *child  = override;
  encoding_config *merge;

  DBG(fprintf(stderr, "config_merge: entered\n"));

  merge = (encoding_config *)ap_pcalloc(p, sizeof(encoding_config));

  if (child->enable_function != ENABLE_FLAG_UNSET)
    merge->enable_function =  child->enable_function;
  else
    merge->enable_function = parent->enable_function;

  DBG(fprintf(stderr,
	      "merged: enable_function == %d\n", merge->enable_function));

  if (child->strip_msaccount != STRIP_FLAG_UNSET)
    merge->strip_msaccount =  child->strip_msaccount;
  else
    merge->strip_msaccount = parent->strip_msaccount;

  DBG(fprintf(stderr,
	      "merged: strip_msaccount == %d\n", merge->strip_msaccount));

  if (child->server_encoding)
    merge->server_encoding =  child->server_encoding;
  else
    merge->server_encoding = parent->server_encoding;

  DBG(fprintf(stderr,
	      "merged: server_encoding == %s\n", merge->server_encoding));

  if (child->default_encoding)
    merge->default_encoding =  child->default_encoding;
  else
    merge->default_encoding = parent->default_encoding;

  merge->client_encoding =
    ap_append_arrays(p, child->client_encoding, parent->client_encoding);

  return merge;
}

/**
 * Handler for encoding conversion.
 *
 * Here, expected encoding by client/server is determined, and
 * whenever needed, client input will be converted to that of
 * server-side expected encoding.
 */
static int
mod_enc_convert(request_rec *r) {
  encoding_config *conf, *dconf, *sconf;

  const char      *oenc; /* server-side encoding */
  array_header    *ienc; /* list of possible encodings */
  void           **list; /* same as above (for iteration) */

  iconv_t cd;            /* conversion descriptor */

  int i;

  LOG(APLOG_DEBUG, r->server, "mod_enc_convert: entered");

  sconf = ap_get_module_config(r->server->module_config, &encoding_module);
  dconf = ap_get_module_config(r->per_dir_config, &encoding_module);
   conf = config_merge(r->pool, sconf, dconf);

  if (conf->enable_function != ENABLE_FLAG_ON) {
    return DECLINED;
  }

  oenc = conf->server_encoding ? conf->server_encoding : "UTF-8";
  ienc = get_client_encoding(r, conf->client_encoding,
			     ap_table_get(r->headers_in, "User-Agent"));

  if (conf->default_encoding)
    ap_array_cat(ienc, conf->default_encoding);

  list = (void **)ienc->elts;

  LOG(APLOG_DEBUG, r->server, "mod_enc_convert: oenc == %s", oenc);

  /* try specified encodings in order */
  for (i = 0 ; i < ienc->nelts ; i++) {
    LOG(APLOG_DEBUG,
	r->server, "mod_enc_convert: ienc <> %s", (char *)list[i]);

    /* pick appropriate converter module */
    if ((cd = iconv_open(oenc, list[i])) == (iconv_t)(-1))
      continue;

    /* conversion tryout */
    if (iconv_header(r, cd) == 0) {
      LOG(APLOG_DEBUG,
	  r->server, "mod_enc_convert: ienc == %s", (char *)list[i]);
      iconv_close(cd);
      return DECLINED;
    }

    /* don't fail, but just continue on until list ends */
    iconv_close(cd);
  }

  LOG(APLOG_WARNING, r->server, "mod_enc_convert: no conversion done");

  return DECLINED;
}

/**
 * Handler for HTTP header parsing.
 *
 * Here, username is normalized and wierd "hostname\" prepended
 * to username is removed (if configured to do so).
 */
static int
mod_enc_parse(request_rec *r) {
  encoding_config *conf, *dconf, *sconf;

  const char *pass;
  char       *user;
  char       *buff;

  LOG(APLOG_DEBUG, r->server, "mod_enc_parse: entered");

  sconf = ap_get_module_config(r->server->module_config, &encoding_module);
  dconf = ap_get_module_config(r->per_dir_config, &encoding_module);
   conf = config_merge(r->pool, sconf, dconf);

  if (conf->enable_function != ENABLE_FLAG_ON) {
    return DECLINED;
  }

  /* Parse header and strip "hostname\" from username - for WinXP */
  if (conf->strip_msaccount == STRIP_FLAG_ON) {

    /* Only basic auth is supported for now */
    if (ap_get_basic_auth_pw(r, &pass) != OK)
      return DECLINED;

    /* Is this username broken? */
    if ((user = index(r->connection->user, '\\')) == NULL)
      return DECLINED;

    /* Re-generate authorization header */
    if (*(user + 1)) {
      buff = ap_pbase64encode(r->pool,
			      ap_psprintf(r->pool, "%s:%s", user + 1, pass));
      ap_table_set(r->headers_in, "Authorization",
		   ap_pstrcat(r->pool, "Basic ", buff, NULL));

      ap_get_basic_auth_pw(r, &pass); /* update */
    }
  }

  return DECLINED;
}

/***************************************************************************
 * exported module structure
 ***************************************************************************/

module MODULE_VAR_EXPORT encoding_module = {
  STANDARD_MODULE_STUFF,
  NULL,             /* initializer */
  folder_setup,     /* dir config */
  config_merge,     /* dir config merger */
  server_setup,     /* server config */
  config_merge,     /* server config merger */
  mod_enc_commands, /* command table */
  NULL,             /* handlers */
  NULL,             /* filename translation */
  NULL,             /* check_user_id */
  NULL,             /* check auth */
  NULL,             /* check access */
  NULL,             /* type_checker */
  NULL,             /* fixups */
  NULL,             /* logger */
  mod_enc_parse,    /* header parser */
  NULL,             /* child_init */
  NULL,             /* child_exit */
  mod_enc_convert,  /* post read-request */
};
