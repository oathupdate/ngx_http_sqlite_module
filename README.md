Nginx Sqlite Module
===================

### Description

`ngx_http_sqlite_module` is a http module which can execute a sql by http_arg, http_body or nginx virables

Install
=============

## install deps

```
yum install -y sqlite-devel
```

## make module install

Compile Nginx with option in `./configure`, as static or dynamic module

```
--add-module=/root/code/ngx_http_sqlite_module
```

if you want to exec sqlite with nginx virables you can compile with ngx-lua-module

```
--add-module=modules/ngx_http_lua_module/
```

Configuration
=============

## http block

```
    sqlite on;      # ngx_http_sqlite_module switch
    sqlite_database /root/nginx/database/test.db;  # sqlite database path
    sqlite_init "create table if not exists test (col1 BIGINT PRIMARY KEY NOT NULL,col2 TEXT NOT NULL);";   # exe some sqls before nginx worker started
    sqlite_init "insert into test ('col1', 'col2') values ('1', 'col_0');";         # exe some sqls before nginx worker started
    sqlite_filter "create";  # if 'create' is a substr of sql, then will not exec this sql
    sqlite_filter "drop";
```

## location block

```
location /test {
      # description:  execute sql which affixed in http uri arg(url?sql=select * from test) or the whole http body
      # http response: this command will response the sql exe result.
       sqlite_exec;
      sqlite_filter "create";  # if 'create' is a substr of sql, then will not exec this sql
}
```

How to get sql exec result
==========================

when a sql has been executed, you can get the result via the bellow towo ways.

1. if the location configed command `sqlite_exec` then you can get result by http response
2. if you set the `ngx.var.sqlite_query` by nginx conf or lua script then you can get result via `ngx.var.sqlite_result`

Sql result format
=================

then result is string of json format. this was s sample

1. sqlite table definition(table name test)

|  col_0   | col_1  |
|  ----  | ----  |
| col_0_0  | col_0_0 |
| col_0_1  | col_0_1 |

2. sql query definition

```
select * from test;
```

3. then the sql exe result will like bellow

```
{
    "count": 1,   // select rows count
    // only status equal success the col_names and rows has value
    "status": "uninitialized/failed/success",
    "col_names": [  // select col name
        "col_0",
        "col_1"
    ],
    "rows": [
        [   
            "col_0_0",  // the value of col_0
            "col_1_0"   // the value of col_1
        ],
        [
            "col_0_1",  // the value of col_0
            "col_1_1"   // the value of col_1
        ]
    ]
}
```
