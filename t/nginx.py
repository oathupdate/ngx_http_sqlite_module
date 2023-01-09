# -*- coding: utf-8 -*-
#! /usr/bin/env python

import os, sys, time
import path

class NgxConf():
    def __init__(self):
        self.global_conf = {
                '01_user': 'root',
                '02_worker_processes': '1',
                '03_events': '{ worker_connections  1024; }'
                }
        self.http_conf = {
            '01_lua_package_path': '"' + path.NGX_LUA_PATH + '?.lua;;"',
            '02_lua_package_cpath': '"' + path.NGX_ROOT_PATH + '/ngx_clua/?.so;;"',
            '03_sqlite': 'on',
            '04_sqlite_database': path.DB_FILE,
            '05_sqlite_init': '"drop table if exists test;"',
            '06_sqlite_init': '"create table if not exists test (col1 INTEGER PRIMARY KEY NOT NULL,col2 TEXT NOT NULL);"',
            '07_sqlite_init': '"insert into test (\'col1\', \'col2\') values (\'1\', \'col_0\');"'
        }

        self.server_conf = {
            '01_listen': '8081',
            '02_server_name': 'localhost default',
        }
        self.local_conf = {
            '01_/test_api': 'sqlite_exec',
            '02_/test_ngx_var': "content_by_lua_file " + path.NGX_LUA_FILE
        }

    def format_val(self, val):
        val = val.strip()
        if val[-1] == '}':
            return val + '\n'
        else:
            return val + ';\n'

    def output_config(self):
        conf = ''
        for key, val in sorted(self.global_conf.items()):
            conf += key[3:] + ' ' + self.format_val(val)

        conf += '\nhttp {\n'
        http_conf_space = '    '
        for key, val in sorted(self.http_conf.items()):
            conf += http_conf_space + key[3:] + ' ' + self.format_val(val)

        server_conf_space = http_conf_space + '    '
        conf += http_conf_space + 'server {\n'
        for key, val in sorted(self.server_conf.items()):
            conf += server_conf_space + key[3:] + ' ' + self.format_val(val)

        local_conf_space = server_conf_space + '    '
        for key, val in sorted(self.local_conf.items()):
            conf += server_conf_space + 'location ' + key[3:] + ' {\n'
            conf += local_conf_space + self.format_val(val) + server_conf_space + '}\n'

        conf += http_conf_space + '}\n'
        conf += '}'
        return conf

    def write_config(self, path):
        file = open(path, 'w')
        file.write(self.output_config())
        file.close()

class Nginx():
    def __init__(self, root_path):
        self.root_path = root_path
        self.pid_file = root_path + 'logs/nginx.pid'
        self.bin_file = root_path + 'sbin/nginx'
        self.conf_file = root_path + 'conf/test_nginx.conf'
        self.start_cmd = '%s -p %s -c %s' % (self.bin_file, self.root_path, self.conf_file)
        self.stop_cmd = self.start_cmd + ' -s stop'
        self.reload_cmd = self.start_cmd + ' -s reload'

    def start(self):
        if os.path.isfile(self.pid_file):
            print('already started')
        else:
            if os.system(self.start_cmd) != 0:
                print('nginx start failed')
        time.sleep(1)

    def stop(self):
        if os.path.isfile(self.pid_file):
            if os.system(self.stop_cmd) != 0:
                print('nginx stop failed')
        else:
            print('nginx is not running')
        time.sleep(0.5)

    def reload(self):
        if os.path.isfile(self.pid_file):
            if os.system(self.reload_cmd) != 0:
                print('nginx reload failed')
        else:
            print('nginx is not running')
       
    def restart(self):
        self.stop()
        self.start()
