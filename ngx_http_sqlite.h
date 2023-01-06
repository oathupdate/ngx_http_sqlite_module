#ifndef _NGX_HTTP_SQLITE_H_INCLUDE_
#define _NGX_HTTP_SQLITE_H_INCLUDE_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <sqlite3.h>

enum {
    EXEC_UNDEFINED,
    EXEC_FAILED,
    EXEC_SUCCESS
};

typedef struct {
    /* sql query exe success or not */
    ngx_int_t           status;
    /* sql query result row count */
    ngx_uint_t          row_count;
    /* sql query result col count */
    ngx_uint_t          col_count;
    /* col_names[ngx_str_t:name] sql query result col name */
    ngx_array_t         col_names;
    /* rows[ngx_array_t:row][ngx_str_t:value] */
    ngx_array_t         rows;
    /* a json format query value */
    ngx_buf_t           serialize_buf;
    ngx_pool_t          *pool;

} ngx_http_sqlite_result_t;

ngx_int_t ngx_http_sqlite_result_reset(ngx_http_sqlite_result_t *res);

ngx_int_t ngx_http_sqlite_exec(sqlite3 *db, ngx_str_t *sql_str,
    ngx_http_sqlite_result_t *res);

ngx_int_t ngx_http_sqlite_result_serialize(ngx_http_sqlite_result_t *res);

#endif  // _NGX_HTTP_SQLITE_H_INCLUDE_
