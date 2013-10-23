#!/bin/sh

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [[ "x$LUA_PATH" == "x" ]]; then LUA_PATH=";;"; fi

if [[ ! -d $DIR ]]; then
    echo "ERROR: failed to detect path to microblx"
fi

UBX_LUA_PATH="$DIR/lua/?.lua"

export LUA_PATH="$UBX_LUA_PATH;$LUA_PATH"
