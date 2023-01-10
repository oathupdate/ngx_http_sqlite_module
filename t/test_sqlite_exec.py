# -*- coding: utf-8 -*-
#! /usr/bin/env python

import unittest
import sqlite3
import requests

import path
import nginx

ngx = nginx.Nginx(path.NGX_ROOT_PATH)

class TestSqliteExec(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.db = sqlite3.connect(path.DB_FILE)
        cls.dbc = cls.db.cursor()
        cls.index = 2048

    @classmethod
    def tearDownClass(cls):
        cls.dbc.close()

    def setUp(self):
        TestSqliteExec.index += 1
        self.index = TestSqliteExec.index

    def test_sqlite_init(self):
        cursor = TestSqliteExec.dbc.execute("select * from test where col1='1'")
        for row in cursor:
            self.assertEqual(row[0], 1)
            self.assertEqual(row[1], 'col_0')

    def test_exec_by_ngx_var(self):
        content = '''
            ngx.var.sqlite_query = "insert into test ('col1', 'col2') values ('%d', 'test_exec_by_ngx_var');"
            ngx.var.sqlite_query = "select * from test where col1 = '%d';"
            ngx.say(tostring(ngx.var.sqlite_result))
            ''' % (self.index, self.index)

        lua = open(path.NGX_LUA_FILE, 'w')
        lua.write(content)
        lua.close()
        ngx.reload()
        content = requests.get(path.HOST + '/test_ngx_var').json()
        self.assertEqual(len(content['rows']), 1)
        self.assertEqual(content['rows'][0][0], str(self.index))
        self.assertEqual(content['rows'][0][1], str('test_exec_by_ngx_var'))
