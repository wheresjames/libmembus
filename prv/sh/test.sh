#!/bin/bash

#--------------------------------------------------------------------------------------------------
TEST=$1
if [ -z $TEST ]; then
    TEST="$SHDIR/libblank-test"
fi

if [ ! -f $TEST ]; then
    TEST="cmake --build ./bld --target libblank-test"
fi

$TEST