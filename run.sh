#!/bin/bash

#--------------------------------------------------------------------------------------------------
    USAGE="
    -------------------------------------------------------
    - libmembus Help                                       -
    -------------------------------------------------------

    USAGE: ${BASH_SOURCE[0]} <command>

    Commands:

        files       - Show installation roots

        help        - Display link to library documentation

        info [var]  - Show installation information
                      var - [optional] Variable to show

        refresh     - Run ldconfig

        test        - Run unit/regression tests

        uninstall   - Uninstall library

    -------------------------------------------------------
    "


#--------------------------------------------------------------------------------------------------
SCRIPTPATH=$(realpath ${BASH_SOURCE[0]})
ROOTDIR=$(dirname $SCRIPTPATH)
if [[ ! -d "$ROOTDIR" ]]; then
    echo "Invalid script path: ${ROOTDIR}"
    exit -1
fi

if [[ -f "${ROOTDIR}/prv/sh/rbashutils.sh" ]]; then
    SHDIR="${ROOTDIR}/prv/sh"
else
    SHDIR=$(realpath ${ROOTDIR}/../share/libmembus)
    if [[ ! -d "$SHDIR" ]]; then
        SHDIR="/usr/share/libmembus"
        if [[ ! -d "$SHDIR" ]]; then
            SHDIR="/usr/local/share/libmembus"
            if [[ ! -d "$SHDIR" ]]; then
                echo "Invalid share path"
                exit -1
            fi
        fi
    fi

fi

if [[ ! -f "${SHDIR}/rbashutils.sh" ]]; then
    echo "Invalid share path"
    exit -1
fi

. "${SHDIR}/rbashutils.sh"

CMD=$1
if [ -z $CMD ]; then
    echo "$USAGE"
    exit
fi

EXEC="$SHDIR/$CMD.sh"
if [[ ! -f "$EXEC" ]]; then
    exitWithError "Invalid command : $CMD"
fi

shift
. $EXEC $@
