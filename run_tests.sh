#!/bin/bash

for t in tests/test_*; do
    echo "running $t"
    luajit $t
done
