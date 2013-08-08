#!/bin/sh
export LUA_PATH="$LUA_PATH;`pwd`/lua/?.lua"
LJIT=`which luajit`

lunit -i $LJIT tests/test_*.lua

