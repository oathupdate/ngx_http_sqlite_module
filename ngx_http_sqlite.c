#include "ngx_http_sqlite_module.h"

ngx_flag_t
ngx_http_sqlite_safty_check(ngx_array_t *black_list, ngx_str_t *sql)
{
    ngx_str_t               *key;
    ngx_uint_t              i;

    key = black_list->elts;
    for (i = 0; i < black_list->nelts; i++) {
        if (sql->len < key[i].len) {
            continue;
        }
        if (ngx_strcasestrn(sql->data, (char*)key[i].data, key[i].len - 1)) {
            ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0,
                          "sql hit blacklist: %V", sql);
            return 0;
        }
    }
    return 1;
}

/*{
  "count": 0,
  "status": "failed/uninitialized/success",
  "col_names":["c0"],
  "rows":[["1"]]
}*/
ngx_int_t
ngx_http_sqlite_result_serialize(ngx_http_sqlite_result_t *res)
{
    ngx_buf_t               *dst = &res->serialize_buf;
    ngx_uint_t              size = 0, i, j;
    ngx_str_t               *val;
    ngx_array_t             *row;
    static const ngx_str_t  err = ngx_string("{\"status\":\"failed\"}"),
                            undef = ngx_string("{\"status\":\"uninitialized\"}"),
                            forrbid = ngx_string("{\"status\":\"forbidden\"}");
    switch (res->status) {
    case EXEC_UNDEFINED:
        dst->pos = undef.data;
        dst->last = dst->pos + undef.len;
        return NGX_OK;
    case EXEC_FAILED:
        dst->pos = err.data;
        dst->last = dst->pos + err.len;
        return NGX_OK;
    case EXEC_FORBIDDEN:
        dst->pos = forrbid.data;
        dst->last = dst->pos + forrbid.len;
        return NGX_OK;
    }

    /* start calculate buf size */
    size += sizeof("{\"status\":\"success\",\"count\": ,}") + NGX_INT_T_LEN;
    size += sizeof("\"col_names\":[],");
    size += sizeof("\"rows\":[]");
    val = res->col_names.elts;
    for (i = 0; i < res->col_names.nelts; i++) {
        size += val[i].len + sizeof("\"\",");
    }
    row = res->rows.elts;
    for (i = 0; i < res->rows.nelts; i++) {
        val = row[i].elts;
        for (j = 0; j < row[i].nelts; j++) {
            size += val[j].len + sizeof("\"\",");
        }
        size += sizeof("[],");
    }
    /* end calculate buf size */
    
    if ((ngx_int_t)size > dst->end - dst->start) {
        dst->pos = dst->start = dst->last = ngx_pnalloc(res->pool, size);
        if (!dst->start) {
            return NGX_ERROR;
        }
        dst->end = dst->start + size;
    } else {
        dst->last = dst->pos = dst->start;
    }
    
    /* start generate json format output */
    dst->last = ngx_sprintf(dst->last, "{\"status\":\"success\",\"count\":%d,",
                            res->row_count);
    dst->last = ngx_sprintf(dst->last, "\"col_names\":[");
    val = res->col_names.elts;
    for (i = 0; i < res->col_names.nelts; i++) {
        dst->last = ngx_sprintf(dst->last, "\"%v\",", &val[i]);
        if (i == res->col_names.nelts - 1) {
            dst->last--;
        }
    }
    dst->last = ngx_sprintf(dst->last, "],\"rows\":[");
    row = res->rows.elts;
    for (i = 0; i < res->rows.nelts; i++) {
        val = row[i].elts;
        dst->last = ngx_sprintf(dst->last, "[");
        for (j = 0; j < row[i].nelts; j++) {
            dst->last = ngx_sprintf(dst->last, "\"%v\",", &val[j]);
            if (j == row[i].nelts - 1) {
                dst->last--;
            }
        }
        dst->last = ngx_sprintf(dst->last, "],");
        if (i == res->rows.nelts - 1) {
            dst->last--;
        }
    }
    dst->last = ngx_sprintf(dst->last, "]}");
    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, "%s", dst->start);

    return NGX_OK;
}

ngx_int_t
ngx_http_sqlite_result_reset(ngx_http_sqlite_result_t *res)
{
    res->row_count = 0;
    res->col_count = 0;
    res->status = EXEC_UNDEFINED;
    res->serialize_buf.pos = res->serialize_buf.start;
    res->serialize_buf.last = res->serialize_buf.pos;

    if (ngx_array_init(&res->col_names, res->pool, 1,
                       sizeof(ngx_str_t)) != NGX_OK) {
        return NGX_ERROR;
    }
    if (ngx_array_init(&res->rows, res->pool, 1,
                       sizeof(ngx_array_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

static int
ngx_http_sqlite_exec_cb(void *data, int argc, char **argv, char **col_names)
{
    int           i;
    ngx_str_t     *name, *val;
    ngx_array_t   *row;

    if (argc == 0 || !data) {
        return NGX_OK;
    }

    ngx_http_sqlite_result_t *res = (ngx_http_sqlite_result_t*)data;
    if (!res->col_count) {
        name = ngx_array_push_n(&res->col_names, argc);
        if (!name) {
            return NGX_ERROR;
        }
        res->col_count = argc;
        for (i = 0; i < argc; i++) {
            name[i].len = ngx_strlen(col_names[i]);
            if (name[i].len) {
                name[i].data = ngx_palloc(res->pool, name[i].len);
                ngx_memcpy(name[i].data, col_names[i], name[i].len);
            }
        }
    }

    row = ngx_array_push(&res->rows);
    if (ngx_array_init(row, res->pool, argc, sizeof(ngx_str_t)) != NGX_OK) {
        return NGX_ERROR;
    }
    val = ngx_array_push_n(row, argc);
    if (!val) {
        return NGX_ERROR;
    }
    for (i = 0; i < argc; i++) {
        val[i].len = argv[i] ? ngx_strlen(argv[i]) : 0;
        if (val[i].len) {
            val[i].data = ngx_palloc(res->pool, val[i].len);
            ngx_memcpy(val[i].data, argv[i], val[i].len);
        }
    }
    res->row_count++;
    return NGX_OK;
}

ngx_int_t
ngx_http_sqlite_exec(sqlite3 *db, ngx_str_t *sql_str,
    ngx_http_sqlite_result_t *res)
{
    char *error = NULL;
    u_char sql[2048], *psql;
    if (res) {
        ngx_http_sqlite_result_reset(res);
        res->status = EXEC_FAILED;
    }
    if (sql_str->len + 1 > sizeof(sql)) {
        psql = ngx_pnalloc(res->pool, sql_str->len + 1);
        if (!psql) {
            return NGX_ERROR;
        }

    } else {
        psql = sql;
    }

    ngx_log_error(NGX_LOG_DEBUG, ngx_cycle->log, 0, "sql_exec: %v", sql_str);

    ngx_sprintf(psql, "%v", sql_str)[0] = '\0';
    if (sqlite3_exec(db, (char*)sql, ngx_http_sqlite_exec_cb, res, &error) !=
        SQLITE_OK) {
        ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0,
                      "sqlite3_exec '%s' error: %s", sql, error);
        sqlite3_free(error);
        return NGX_ERROR;
    }
    if (res) {
        res->status = EXEC_SUCCESS;
    }
    return NGX_OK;
}
