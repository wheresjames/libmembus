#!/bin/bash

USAGE="USAGE: ${BASH_SOURCE[0]} [conan|docker|gradle|pip|npm]"

#--------------------------------------------------------------------------------------------------
SCRIPTPATH=$(realpath ${BASH_SOURCE[0]})
ROOTDIR=$(dirname $SCRIPTPATH)
if [[ ! -d "$ROOTDIR" ]]; then
    echo "Invalid script path: ${ROOTDIR}"
    exit -1
fi
. "${ROOTDIR}/prv/sh/rbashutils.sh"

ADD=$1
if [ -z $ADD ]; then
    exitWithError "$USAGE"
fi

TGTD="$ROOTDIR"
SRCD="${ROOTDIR}/prv/ext/${ADD}"
if [ ! -d "$SRCD" ]; then
    exitWithError "Unrecognized extension: $ADD"
fi



# Does this extension have source files?
if [[ -d "${SRCD}/src" ]] && [[ "-y" != "$2" ]]; then

    echo "This extension contains new source files."
    echo "If you choose to continue, any existing source files will be moved to ./old"
    if ! askYesNo "Do you want to continue"; then
        doExit 0
    fi

fi

# For each file
FILES="${SRCD}/*"
for SRC in $FILES; do

    FNAME=`basename $SRC`
    TGT=$TGTD/$FNAME

    # showInfo "$SRC -> $TGT"
    # showInfo "Install: $FNAME"

    # Is it an empty directory?
    if [[ "$SRC" =~ "*" ]]; then
        EMPTY=

    # Is it a directory
    elif [[ -d "$SRC" ]]; then

        # Does this extension have source files?
        if [[ "src" == "$FNAME" ]]; then

            # Where to backup existing source files
            BKT="old/src.$(date +'%Y-%m-%d-%-H%M%S')"
            BKD="${TGTD}/${BKT}"
            if [[ -d "$BKD" ]]; then
                exitWithError "Directory exists: ${BKD}"
            fi

            # Move existing source files
            DIRS="${TGTD}/src/*"
            for TD in $DIRS; do

                if [[ "$TD" =~ "*" ]]; then
                    EMPTY=

                else

                    DNAME=`basename $TD`

                    if [[ ! -d "$BKD" ]]; then
                        mkdir -p "$BKD"
                        exitOnError "Failed to create directory ${BKD}"
                        showWarning "Moving existing source files to : ${BKT}"
                    fi

                    BD="${BKD}/${DNAME}"
                    showWarning "Moving: ${FNAME}/${DNAME} -> ${BKT}/${DNAME}"

                    mv "$TD" "${BD}"

                fi

            done

            # Copy new source folders
            DIRS="${SRC}/*"
            for SD in $DIRS; do

                if [[ "$SD" =~ "*" ]]; then
                    EMPTY=

                else
                    DNAME=`basename $SD`

                    if [[ ! -d "$TGT" ]]; then
                        mkdir -p "$TGT"
                    fi

                    TD=$TGT/$DNAME
                    if [[ -d "$TD" ]]; then
                        exitWithError "Already exists: ${TD}"
                    fi

                    showInfo "Install: ${FNAME}/${DNAME}"

                    cp -rn "$SD" "$TD"
                fi

            done

        else
            showInfo "Install: $FNAME"
            if [[ -d $TGT ]]; then
                showWarning "Directory already exists: $TGT"
            else
                cp -rn "$SRC" "$TGT"
            fi
        fi

    # Process file
    elif [[ -f "$SRC" ]]; then

        if [[ -f $TGT ]]; then
            showWarning "File already exists: $FNAME"
        else
            showInfo "Install: $FNAME"
            cp "$SRC" "$TGT"
        fi
    fi

done
