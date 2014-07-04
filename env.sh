#!/bin/sh


SOURCE="${BASH_SOURCE[0]}"

# resolve $SOURCE until the file is no longer a symlink
while [ -h "$SOURCE" ]; do
    cd -P "$( dirname "$SOURCE" )" > /dev/null 2>&1
    DIR="$( pwd )"
    SOURCE="$(readlink "$SOURCE")"
    # If $SOURCE was a relative symlink,
    # we need to resolve it relative to the 
    # path where the symlink file was located.
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done

cd -P  "$( dirname "$SOURCE" )" > /dev/null 2>&1
DIR="$( pwd )"


if [[ "x$LUA_PATH" == "x" ]]; then LUA_PATH=";;"; fi

if [[ ! -d $DIR ]]; then
    echo "ERROR: failed to detect path to microblx: $DIR"
fi

UBX_LUA_PATH="$DIR/lua/?.lua"

export LUA_PATH="$UBX_LUA_PATH;$LUA_PATH"
