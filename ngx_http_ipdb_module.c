/*
 * Copyright (C) vislee
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ipdb/ipdb.h"

#define NGX_IPDB_country_name    0
#define NGX_IPDB_region_name     1
#define NGX_IPDB_city_name       2
#define NGX_IPDB_owner_domain    3
#define NGX_IPDB_isp_domain      4
#define NGX_IPDB_latitude        5
#define NGX_IPDB_longitude       6

typedef struct {
    ipdb_reader    *ipdb;
    ngx_str_t       lang;
} ngx_http_ipdb_conf_t;


static char *ngx_http_ipdb_language(ngx_conf_t *cf, void *post, void *data);
static ngx_int_t ngx_http_ipdb_add_variables(ngx_conf_t *cf);
static void *ngx_http_ipdb_create_conf(ngx_conf_t *cf);
static char *ngx_http_ipdb_init_conf(ngx_conf_t *cf, void *conf);
static void ngx_http_ipdb_cleanup(void *data);
static char *ngx_http_ipdb_open(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_ipdb_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


static ngx_conf_post_handler_pt  ngx_http_ipdb_language_p =
    ngx_http_ipdb_language;


static ngx_command_t  ngx_http_ipdb_commands[] = {

    { ngx_string("ipdb"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE12,
      ngx_http_ipdb_open,
      NGX_HTTP_MAIN_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("ipdb_language"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_ipdb_conf_t, lang),
      &ngx_http_ipdb_language_p },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_ipdb_module_ctx = {
    ngx_http_ipdb_add_variables,          /* preconfiguration */
    NULL,                                 /* postconfiguration */

    ngx_http_ipdb_create_conf,            /* create main configuration */
    ngx_http_ipdb_init_conf,              /* init main configuration */

    NULL,                                 /* create server configuration */
    NULL,                                 /* merge server configuration */

    NULL,                                 /* create location configuration */
    NULL                                  /* merge location configuration */
};


ngx_module_t  ngx_http_ipdb_module = {
    NGX_MODULE_V1,
    &ngx_http_ipdb_module_ctx,            /* module context */
    ngx_http_ipdb_commands,               /* module directives */
    NGX_HTTP_MODULE,                      /* module type */
    NULL,                                 /* init master */
    NULL,                                 /* init module */
    NULL,                                 /* init process */
    NULL,                                 /* init thread */
    NULL,                                 /* exit thread */
    NULL,                                 /* exit process */
    NULL,                                 /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_variable_t  ngx_http_ipdb_vars[] = {

    { ngx_string("ipdb_country_name"), NULL,
      ngx_http_ipdb_variable,
      NGX_IPDB_country_name, 0, 0 },

    { ngx_string("ipdb_region_name"), NULL,
      ngx_http_ipdb_variable,
      NGX_IPDB_region_name, 0, 0 },

    { ngx_string("ipdb_city_name"), NULL,
      ngx_http_ipdb_variable,
      NGX_IPDB_city_name, 0, 0 },

    { ngx_string("ipdb_isp_domain"), NULL,
      ngx_http_ipdb_variable,
      NGX_IPDB_isp_domain, 0, 0 },

      ngx_http_null_variable
};


static char *
ngx_http_ipdb_language(ngx_conf_t *cf, void *post, void *data)
{
    ngx_str_t  *lang = data;

    if (ngx_strcmp(lang->data, "EN") == 0
        || ngx_strcmp(lang->data, "CN") == 0) {

        return NGX_CONF_OK;
    }

    return NGX_CONF_ERROR;
}


static ngx_int_t
ngx_http_ipdb_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_ipdb_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}


static void *
ngx_http_ipdb_create_conf(ngx_conf_t *cf)
{
    ngx_pool_cleanup_t     *cln;
    ngx_http_ipdb_conf_t   *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_ipdb_conf_t));
    if (conf == NULL) {
        return NULL;
    }


    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NULL;
    }

    cln->handler = ngx_http_ipdb_cleanup;
    cln->data = conf;

    return conf;
}


static char *
ngx_http_ipdb_init_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_ipdb_conf_t  *icf = conf;

    if (!icf->lang.data) {
        ngx_str_set(&icf->lang, "EN");
    }

    return NGX_CONF_OK;
}


static void
ngx_http_ipdb_cleanup(void *data)
{
    ngx_http_ipdb_conf_t  *icf = data;

    if (icf->ipdb) {
        ipdb_reader_free(&icf->ipdb);
    }
}


static char *
ngx_http_ipdb_open(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_int_t              err;
    ngx_http_ipdb_conf_t  *icf = conf;

    ngx_str_t  *value;

    if (icf->ipdb) {
        return "is duplicate";
    }

    value = cf->args->elts;

    err = ipdb_reader_new((char *) value[1].data, &icf->ipdb);

    if (err || icf->ipdb == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "ipdb_reader_new(\"%V\") failed", &value[1]);

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

// "a\tb\t\tc"
// ""
static char *
ngx_http_ipdb_get_index_item(char *v, ngx_int_t idx)
{
    char *p, *q;
    ngx_int_t n = 0;

    if (v == NULL) return NULL;

    p = v;
    q = p;

    while (*p) {

        if (*p == '\t') {
            *p = 0;
            if (idx == n) {
                break;
            }

            ++p;
            q = p;
            ++n;
        }

        ++p;
    }

    if (idx == n || idx == n+1) {
        return q;
    }

    return NULL;
}


static ngx_int_t
ngx_http_ipdb_item_by_addr(ipdb_reader *reader, ngx_addr_t *addr,
    const char *lang, char *body)
{
    int node = 0;
    ngx_int_t err, i;
    off_t off = -1;
    size_t p = 0, o = 0, s = 0, e = 0;
    size_t len = reader->meta->fields_length;
    const char *content;

    for (i = 0; i < reader->meta->language_length; ++i) {
        if (ngx_strcmp(lang, reader->meta->language[i].name) == 0) {
            off = reader->meta->language[i].offset;
            break;
        }
    }

    if (off == -1) {
        return ErrNoSupportLanguage;
    }

    if (addr->sockaddr->sa_family == AF_INET) {
        if (!ipdb_reader_is_ipv4_support(reader)) {
            return ErrNoSupportIPv4;
        }

        struct sockaddr_in  *sin;

        sin = (struct sockaddr_in *) addr->sockaddr;
        // sin->sin_addr.s_addr
        err = ipdb_search(reader, (const u_char *) &sin->sin_addr.s_addr, 32, &node);
        if (err != ErrNoErr) {
            return err;
        }
    }
#if (NGX_HAVE_INET6)
    else if (addr->sockaddr->sa_family == AF_INET6) {
        if (!ipdb_reader_is_ipv6_support(reader)) {
            return ErrNoSupportIPv6;
        }

        struct in6_addr  *inaddr6;
        inaddr6 = &((struct sockaddr_in6 *) addr->sockaddr)->sin6_addr;
        err = ipdb_search(reader, (const u_char *) &inaddr6->s6_addr, 128, &node);
        if (err != ErrNoErr) {
            return err;
        }
    }
#endif
    else {
        return ErrIPFormat;
    }

    err = ipdb_resolve(reader, node, &content);
    if (err) {
        return err;
    }

    while (*(content + p)) {
        if (*(content + p) == '\t') {
            ++o;
        }
        if ((!e) && o == off + len) {
            e = p;
        }
        ++p;
        if (off && (!s) && (off_t)o == off) {
            s = p;
        }
    }
    if (!e) e = p;
    if (off + len > o + 1) {
        err = ErrDatabaseError;
    } else {
        strncpy(body, content + s, e - s);
        body[e - s] = 0;
    }

    return err;
}

static ngx_int_t
ngx_http_ipdb_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    size_t                  len;
    char                   *p;
    char                    body[512];
    ngx_int_t               err;
    ngx_addr_t              addr;
    ngx_http_ipdb_conf_t   *icf;

#if (NGX_DEBUG)
    ngx_str_t               debug;
#endif

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "ngx_http_ipdb_variable");

    icf = ngx_http_get_module_main_conf(r, ngx_http_ipdb_module);

    if (icf->ipdb == NULL) {
        goto not_found;
    }

    // err = ipdb_reader_find(icf->ipdb, "36.102.4.81", (const char *)icf->lang.data, body);
    addr.sockaddr = r->connection->sockaddr;
    addr.socklen = r->connection->socklen;
    err = ngx_http_ipdb_item_by_addr(icf->ipdb, &addr, (const char *)icf->lang.data, body);
    if (err) {
        goto not_found;
    }

#if (NGX_DEBUG)
    debug.len = ngx_strlen(body);
    debug.data = (u_char *)body;
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "ngx_http_ipdb_variable, item: \"%V\"", &debug);
#endif

    p = ngx_http_ipdb_get_index_item(body, data);
    if (p == NULL) {
        goto not_found;
    }
    len = ngx_strlen(p);

#if (NGX_DEBUG)
    debug.len = len;
    debug.data = (u_char *)p;
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "ngx_http_ipdb_variable, %l res: \"%V\"", data, &debug);
#endif

    // ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
    //     "=========ngx_http_ipdb_variable. res: %s", p);

    v->data = ngx_pnalloc(r->pool, len);
    if (v->data == NULL) {
        return NGX_ERROR;
    }

    ngx_memcpy(v->data, p, len);

    v->len = len;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;

not_found:

    v->not_found = 1;

    return NGX_OK;
}
