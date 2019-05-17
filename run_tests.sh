#!/bin/bash

for t in tests/test_*.lua; do
    echo "running $t"
    luajit $t || exit $?
    echo "----------------------------------------------------------------"
done
