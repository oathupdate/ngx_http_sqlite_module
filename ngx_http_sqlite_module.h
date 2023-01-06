#ifndef _NGX_HTTP_SQLITE_MODULE_H_INCLUDE_
#define _NGX_HTTP_SQLITE_MODULE_H_INCLUDE_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_sqlite.h"

typedef struct {
    ngx_flag_t                         enable;

} ngx_http_sqlite_loc_conf_t;

typedef struct {
    /* if has one loc conf enable then main config enable = true */
    ngx_flag_t                         enable;
    /* database path */
    ngx_str_t                          database;
    /* some sql queries should be exacuted before running */
    ngx_array_t                        *init_sqls;
    sqlite3                            *db;

} ngx_http_sqlite_main_conf_t;

typedef struct {
    ngx_flag_t                enable;
    ngx_str_t                 query;
    ngx_http_sqlite_result_t  result;

} ngx_http_sqlite_ctx_t;

#endif  // _NGX_HTTP_SQLITE_MODULE_H_INCLUDE_
