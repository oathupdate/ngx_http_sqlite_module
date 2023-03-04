# -*- coding: utf-8 -*-
#! /usr/bin/env python

import sys, os
import unittest

import nginx
import path

os.chdir(sys.path[0])

if not os.path.exists(path.DB_PATH):
    os.mkdir(path.DB_PATH)
if not os.path.exists(path.NGX_LUA_PATH):
    os.mkdir(path.NGX_LUA_PATH)
open(path.NGX_LUA_FILE, 'w').close()

nginx.NgxConf().write_config(path.NGX_CONF_FILE)
ngx = nginx.Nginx(path.NGX_ROOT_PATH)

discover = unittest.defaultTestLoader.discover(sys.path[0], pattern="test_*.py")

if __name__ == "__main__":
    ngx.start()
    runner = unittest.TextTestRunner()
    runner.run(discover)
    ngx.stop()
    os.system('rm -f *.pyc')
