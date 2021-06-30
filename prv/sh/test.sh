#!/bin/bash

#--------------------------------------------------------------------------------------------------
TEST=$1
if [ -z $TEST ]; then
    TEST="$SHDIR/libmembus-test"
fi

if [ ! -f $TEST ]; then
    TEST="cmake --build ./bld --target libmembus-test"
fi

$TEST