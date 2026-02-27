#!/bin/bash
exec luajit tests/run_all_tests.lua "$@"
