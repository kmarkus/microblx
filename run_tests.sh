#!/bin/bash

for t in tests/test_*; do
    luajit $t
done
