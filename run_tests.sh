#!/bin/sh
export LUA_PATH="$LUA_PATH;`pwd`/lua/?.lua"
LJIT=`which luajit`

for t in tests/test_*.lua; do
    echo "executing test $t"
    lunit -i $LJIT $t
done

