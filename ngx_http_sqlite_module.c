#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_sqlite_module.h"

/* ngx_http_sqlite_commands functions */
static char *ngx_http_sqlite_exec_slot(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

/* ngx_http_sqlite_vars functions */
static ngx_int_t ngx_http_sqlite_result_get(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static void ngx_http_sqlite_query_set(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);
static ngx_int_t ngx_http_sqlite_query_get(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

/* ngx_http_sqlite_module_ctx fcuntions */
static ngx_int_t ngx_http_sqlite_add_variables(ngx_conf_t *cf);
static void *ngx_http_sqlite_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_sqlite_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_sqlite_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_sqlite_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

/* ngx_http_sqlite_module functions */
static ngx_int_t ngx_http_sqlite_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_sqlite_init_worker(ngx_cycle_t *cycle);
static void ngx_http_sqlite_exit_worker(ngx_cycle_t *cycle);

/* context run functions */
static ngx_int_t ngx_http_sqlite_exec_handler(ngx_http_request_t *r);
static ngx_http_sqlite_ctx_t *ngx_http_sqlite_get_ctx(ngx_http_request_t *r);
static ngx_int_t ngx_http_sqlite_send_response(ngx_http_request_t *r);


static ngx_command_t  ngx_http_sqlite_commands[] = {

    { ngx_string("sqlite"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_sqlite_loc_conf_t, enable),
      NULL
    },

    { ngx_string("sqlite_database"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_sqlite_main_conf_t, database),
      NULL
    },

    { ngx_string("sqlite_init"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_array_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_sqlite_main_conf_t, init_sqls),
      NULL
    },

    { ngx_string("sqlite_exec"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_sqlite_exec_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_sqlite_main_conf_t, init_sqls),
      NULL
    },

    ngx_null_command
};

static ngx_http_variable_t ngx_http_sqlite_vars[] = { 
    { ngx_string("sqlite_query"),
      ngx_http_sqlite_query_set, ngx_http_sqlite_query_get, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_CHANGEABLE|NGX_HTTP_VAR_WEAK, 0 },

    { ngx_string("sqlite_result"),
      NULL, ngx_http_sqlite_result_get, 0,
      NGX_HTTP_VAR_NOCACHEABLE|NGX_HTTP_VAR_CHANGEABLE, 0 },

    ngx_http_null_variable
};

static ngx_http_module_t  ngx_http_sqlite_module_ctx = {
    ngx_http_sqlite_add_variables,      /* proconfiguration */
    NULL,                               /* postconfiguration */
    ngx_http_sqlite_create_main_conf,   /* create main configuration */
    ngx_http_sqlite_init_main_conf,     /* init main configuration */
    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */
    ngx_http_sqlite_create_loc_conf,    /* create location configuration */
    ngx_http_sqlite_merge_loc_conf      /* merge location configuration */
};


ngx_module_t  ngx_http_sqlite_module = {
    NGX_MODULE_V1,
    &ngx_http_sqlite_module_ctx,        /* module context */
    ngx_http_sqlite_commands,           /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    ngx_http_sqlite_init_module,        /* init module */
    ngx_http_sqlite_init_worker,        /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    ngx_http_sqlite_exit_worker,        /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};

static char *
ngx_http_sqlite_exec_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_sqlite_exec_handler;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_sqlite_result_get(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_http_sqlite_ctx_t        *ctx;
    ngx_http_sqlite_result_t     *res;

    ctx = ngx_http_sqlite_get_ctx(r);
    if (!ctx || !ctx->enable) {
        goto not_found;
    }
    
    res = &ctx->result;
    if (ngx_http_sqlite_result_serialize(res) != NGX_OK) {
        goto not_found;
    }
    v->len = res->serialize_buf.last - res->serialize_buf.pos;
    v->data = res->serialize_buf.pos;
    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;

not_found:
    v->valid = 0;
    v->not_found = 1;
    return NGX_OK;
}

static void
ngx_http_sqlite_query_set(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_http_sqlite_ctx_t        *ctx;

    ctx = ngx_http_sqlite_get_ctx(r);
    if (!ctx || !ctx->enable) {
        return;
    }

    ctx->query.data = v->data;
    ctx->query.len = v->len;

    ngx_http_sqlite_exec(ctx->db, &ctx->query, &ctx->result);
}

static ngx_int_t
ngx_http_sqlite_query_get(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
    ngx_http_sqlite_ctx_t        *ctx;

    ctx = ngx_http_sqlite_get_ctx(r);
    if (!ctx || !ctx->enable) {
        v->valid = 0;
        v->not_found = 1; 
        return NGX_OK;
    }
    
    v->len = ctx->query.len;
    v->data = ctx->query.data;
    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;
    return NGX_OK;
}

static ngx_int_t
ngx_http_sqlite_add_variables(ngx_conf_t *cf)
{
    ngx_int_t             index;
    ngx_http_variable_t   *var, *v;

    for (v = ngx_http_sqlite_vars; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }
        
        var->get_handler = v->get_handler;
        var->set_handler = v->set_handler;
        var->data = v->data;
        index = ngx_http_get_variable_index(cf, &v->name);
        if (index == NGX_ERROR) {
            return NGX_ERROR;
        }
        var->index = index;
    }
    
    return NGX_OK;
}

static void *
ngx_http_sqlite_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_sqlite_main_conf_t   *smcf;

    smcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlite_main_conf_t));
    if (!smcf) {
        return NULL;
    }
    smcf->init_sqls = ngx_pcalloc(cf->pool, sizeof(ngx_array_t));
    if (!smcf->init_sqls) {
        return NULL;
    }
    if (ngx_array_init(smcf->init_sqls, cf->pool, 1, sizeof(ngx_str_t)) !=
        NGX_OK) {
        return NULL;
    }

    smcf->enable = NGX_CONF_UNSET;
    return smcf;
}


static char *
ngx_http_sqlite_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_sqlite_main_conf_t   *smcf = conf;
    if (smcf->enable == NGX_CONF_UNSET) {
        smcf->enable = 0;
    }
    if (!smcf->database.len) {
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "sqlite_database not configed");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static void *
ngx_http_sqlite_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_sqlite_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sqlite_loc_conf_t));
    conf->enable = NGX_CONF_UNSET;

    return conf;
}

static char *
ngx_http_sqlite_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_sqlite_loc_conf_t    *prev = parent;
    ngx_http_sqlite_loc_conf_t    *conf = child;
    ngx_http_sqlite_main_conf_t   *smcf;

    smcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_sqlite_module);

    ngx_conf_merge_value(conf->enable, prev->enable, 1);
    if (conf->enable) {
        smcf->enable = 1;
    }

    return NGX_CONF_OK;
}

static ngx_buf_t *
ngx_http_sqlite_read_body(ngx_http_request_t *r)
{
    size_t        size;
    ngx_int_t     len;
    ngx_buf_t    *body;
    ngx_chain_t  *cl;

    if (r->request_body == NULL || r->request_body->bufs == NULL) {
        return NULL;
    }

    for (cl = r->request_body->bufs; cl; cl = cl->next) {
        len += ngx_buf_size(cl->buf);
    }
    if (!len) {
        return NULL;
    }

    body = ngx_create_temp_buf(r->pool, len);
    if (!body) {
        return NULL;
    }

    for (cl = r->request_body->bufs; cl; cl = cl->next) {
        size = ngx_buf_size(cl->buf);

        if (cl->buf->in_file) {
            len = ngx_read_file(cl->buf->file, body->last,
                                size, cl->buf->file_pos);
            if (len == NGX_ERROR) { 
                return NULL;
            }
            body->last += len;
        } else {
            body->last = ngx_copy(body->last, cl->buf->pos, size);
        }
    }

    return body;
}


static ngx_int_t
ngx_http_sqlite_init_module(ngx_cycle_t *cycle)
{
    sqlite3                      *db;
    ngx_str_t                    *sql;
    ngx_uint_t                   i;
    ngx_http_sqlite_main_conf_t  *smcf;

    smcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_sqlite_module);

    if (!smcf || !smcf->enable) {
        return NGX_OK;
    }

    if (sqlite3_open((const char *)smcf->database.data, &db)) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "sqlite3_open %V error: %s",
                      &smcf->database, sqlite3_errmsg(db));
        sqlite3_close(db);
        return NGX_ERROR;
    }


    sql = smcf->init_sqls->elts;
    for (i = 0; i < smcf->init_sqls->nelts; i++) {
        if (ngx_http_sqlite_exec(db, &sql[i], NULL) != NGX_OK) {
            sqlite3_close(db);
            return NGX_ERROR;
        }
    }

    sqlite3_close(db);
    return NGX_OK;
}

static ngx_int_t
ngx_http_sqlite_init_worker(ngx_cycle_t *cycle)
{
    ngx_http_sqlite_main_conf_t  *smcf;

    smcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_sqlite_module);

    if (!smcf || !smcf->enable) {
        return NGX_OK;
    }

    if (sqlite3_open((const char *)smcf->database.data, &smcf->db)) {
        ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "sqlite3_open %V error: %s",
                      &smcf->database, sqlite3_errmsg(smcf->db));
        return NGX_ERROR;
    }

    sqlite3_busy_timeout(smcf->db, 100);

    return NGX_OK;
}

static void
ngx_http_sqlite_exit_worker(ngx_cycle_t *cycle)
{
    ngx_http_sqlite_main_conf_t  *smcf;

    smcf = ngx_http_cycle_get_module_main_conf(ngx_cycle, ngx_http_sqlite_module);

    if (!smcf || !smcf->enable) {
        return;
    }

    sqlite3_close(smcf->db);
}

static ngx_http_sqlite_ctx_t *
ngx_http_sqlite_get_ctx(ngx_http_request_t *r)
{
    ngx_http_sqlite_ctx_t         *ctx;
    ngx_http_sqlite_loc_conf_t    *sloc;
    ngx_http_sqlite_main_conf_t   *smcf;

    ctx = ngx_http_get_module_ctx(r, ngx_http_sqlite_module);

    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_sqlite_ctx_t));
        if (ctx == NULL) {
            return NULL;
        }

        smcf = ngx_http_get_module_main_conf(r, ngx_http_sqlite_module);
        sloc = ngx_http_get_module_loc_conf(r, ngx_http_sqlite_module);
        ctx->enable = sloc->enable;
        ctx->db = smcf->db;
        ctx->result.pool = r->pool;
        ctx->result.status = EXEC_UNDEFINED;
        ngx_http_set_ctx(r, ctx, ngx_http_sqlite_module);
    }

    return ctx;
}

static void
ngx_http_sqlite_body_post_read(ngx_http_request_t *r)
{
    ngx_buf_t                    *body;
    ngx_http_sqlite_ctx_t        *ctx;

    body = ngx_http_sqlite_read_body(r);
    if (!body) {
        goto end;
    }
    ctx = ngx_http_sqlite_get_ctx(r);
    ctx->query.data = body->pos;
    ctx->query.len = body->last - body->pos;
    ngx_http_sqlite_exec(ctx->db, &ctx->query, &ctx->result);

end:
    ngx_http_finalize_request(r, ngx_http_sqlite_send_response(r));
}

static ngx_int_t
ngx_http_sqlite_exec_handler(ngx_http_request_t *r)
{
    u_char                       *dst, *src;
    ngx_int_t                     rc;
    ngx_str_t                     arg_key = ngx_string("sql"), sql;
    ngx_http_sqlite_ctx_t        *ctx;

    ctx = ngx_http_sqlite_get_ctx(r);
    if (!ctx || !ctx->enable) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "run exec handler but module not enable");
        return NGX_ERROR;
    }

    if (NGX_OK == ngx_http_arg(r, arg_key.data, arg_key.len, &sql)) {
        ctx->query.data = ngx_palloc(r->pool, sql.len);
        if (!ctx->query.data) {
            return NGX_ERROR;
        }
        dst = ctx->query.data;
        src = sql.data;
        ngx_unescape_uri(&dst, &src, sql.len, 0);
        ctx->query.len = dst - ctx->query.data;
        ngx_http_sqlite_exec(ctx->db, &ctx->query, &ctx->result);
        return ngx_http_sqlite_send_response(r);
    }
    rc = ngx_http_read_client_request_body(r, ngx_http_sqlite_body_post_read);
    if (rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    return NGX_DONE;
}

static ngx_int_t
ngx_http_sqlite_send_response(ngx_http_request_t *r)
{
    ngx_int_t                     rc;
    ngx_buf_t                    *buf;
    ngx_chain_t                   out;
    ngx_http_sqlite_ctx_t        *ctx;

    ctx = ngx_http_sqlite_get_ctx(r);
    if (!ctx || !ctx->enable) {
        return NGX_ERROR;
    }
    buf = &ctx->result.serialize_buf;
    if (buf->last - buf->pos == 0) {
        ngx_http_sqlite_result_serialize(&ctx->result);
    }
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = buf->last - buf->pos;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }


    buf->last_buf = 1;
    buf->last_in_chain = 1;
    buf->temporary = 1;

    out.buf = buf;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}
