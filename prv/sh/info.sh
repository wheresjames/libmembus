#!/bin/bash

if [ -f "${SHDIR}/info.txt" ]; then
    INFOFILE="${SHDIR}/info.txt"

elif [ -f "${ROOTDIR}/bld/info.txt" ]; then
    INFOFILE="${ROOTDIR}/bld/info.txt"

else
    exitWithError "Installation information not found"

fi

VAR=$1

if [[ -z $VAR ]] || [[ "$VAR" == "all" ]]; then
    cat "$INFOFILE"
    exit 0
fi

while IFS= read -r LINE; do

    PARTS=(${LINE// / })
    if [[ "$VAR" == "${PARTS[0]}" ]]; then
        echo ${PARTS[@]:1}
        exit 0
    fi

done < "$INFOFILE"
