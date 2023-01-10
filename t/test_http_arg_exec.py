# -*- coding: utf-8 -*-
#! /usr/bin/env python

import unittest
import sqlite3
import requests

import path
import nginx

ngx = nginx.Nginx(path.NGX_ROOT_PATH)

class TestHttpArgExec(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index = 1024

    def setUp(self):
        TestHttpArgExec.index += 1
        self.index = TestHttpArgExec.index

    def test_uri_arg_exec(self):
        insert_sql = "insert into test ('col1', 'col2') values ('%d', 'test_uri_arg_exec');" % (self.index)
        select_sql = "select * from test where col1='%d'" % (self.index)
        requests.get(path.HOST + '/test_arg?sql=' + insert_sql)
        content = requests.get(path.HOST + '/test_arg?sql=' + select_sql).json()
        self.assertEqual(len(content['rows']), 1)
        self.assertEqual(content['rows'][0][0], str(self.index))
        self.assertEqual(content['rows'][0][1], str('test_uri_arg_exec'))

    def test_body_exec(self):
        insert_sql = "insert into test ('col1', 'col2') values ('%d', 'test_body_exec');" % (self.index)
        select_sql = "select * from test where col1='%d'" % (self.index)
        requests.post(path.HOST + '/test_arg', data = insert_sql)
        content = requests.post(path.HOST + '/test_arg', data = select_sql).json()
        self.assertEqual(len(content['rows']), 1)
        self.assertEqual(content['rows'][0][0], str(self.index))
        self.assertEqual(content['rows'][0][1], str('test_body_exec'))
