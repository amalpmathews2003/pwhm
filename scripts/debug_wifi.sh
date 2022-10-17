#!/bin/sh

LIBDEBUG=/usr/lib/debuginfo/debug_functions.sh

if [ ! -f "$LIBDEBUG" ]; then
    echo "No debug library found : $LIBDEBUG"
    exit 1
fi

. $LIBDEBUG
