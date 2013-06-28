#!/bin/sh

LJIT=`which luajit`
lunit -i $LJIT tests/test_data_init.lua
