#!/bin/bash

#--------------------------------------------------------------------------------------------------
if findIn "dpkg -l" "libmembus"; then
    dpkg -r libmembus
    exitOnError "Failed to uninstall"
    exit 0
fi

#--------------------------------------------------------------------------------------------------
INFILE=$1
if [ -z $INDIR ]; then
    INFILE="$SHDIR/MANIFEST.txt"
    if [ ! -f "${INFILE}" ]; then
        INFILE="${ROOTDIR}/bld/install_manifest.txt"
    fi
    if [ ! -f "${INFILE}" ]; then
        INFILE="${ROOTDIR}/bld/MANIFEST.txt"
    fi
    if [ ! -f "${INFILE}" ]; then
        INFILE="${ROOTDIR}/prv/install/MANIFEST.txt"
    fi
fi

# Try dpkg if failed to find manifest
if [ ! -f "${INFILE}" ]; then

    if [ -f "$SHDIR/PYTHON.txt" ]; then
        if [ -f `which python3` ]; then
            python3 -m pip uninstall -y libmembus
        elif [ -f `which python` ]; then
            python -m pip uninstall -y libmembus
        fi
    fi

    if [ "$EUID" == "0" ]; then
        if [ -f `which dpkg` ]; then
            dpkg -r libmembus
        fi
    fi

    exit 0
fi

showInfo "Manifest: $INFILE"

#--------------------------------------------------------------------------------------------------
# Get installation files
FILES=$(<${INFILE})

# Build a list of directories
DIRS=
for v in $FILES; do

    d="$v"
    if [ -f "$d" ]; then
        d=$(dirname "$d")
    fi

    if [ -d "$d" ]; then
        if [[ " $DIRS " != *" $d "* ]]; then
            DIRS="$DIRS $d"
        fi
    fi

done

# Library name (./rename.sh will change this)
LIBNAME="libmembus"

# /usr/bin/${LIBNAME}
MINSAFESIZE=$((9 + ${#LIBNAME}))

# Remove files
showInfo "Cleaning up files"
for v in $FILES; do
    STATUS=" "
    if [ ${#v} -lt ${MINSAFESIZE} ]; then
        STATUS="<"
    # elif [ "/usr/" != "${v:0:5}" ]; then
    #     STATUS="/"
    elif [ -f "$v" ]; then
        STATUS="-"
        rm "$v"
    elif [ -L "$v" ]; then
        STATUS="%"
        rm "$v"
    elif [ -d "$v" ]; then
        STATUS="!"
        rm -Rf "$v"
    fi
    echo "[$STATUS] $v"
done

# Remove empty directories
showInfo "Cleaning up directories"
for v in $DIRS; do
    STATUS=" "
    if [ ${#v} -lt ${MINSAFESIZE} ]; then
        STATUS="<"
    # elif [ "/usr/" != "${v:0:5}" ]; then
    #     STATUS="/"
    elif [ -d "$v" ]; then
        STATUS="*"
        if [ ! "$(ls -A $v)" ]; then
            STATUS="-"
            rmdir -p --ignore-fail-on-non-empty "$v"
        fi
    fi
    echo "[$STATUS] $v"
done

showInfo "Running hash -r"
hash -r

# https://github.com/esnet/iperf/issues/153
if [ "$EUID" == "0" ]; then
    showWarning "You may need to run \"hash -r\" without sudo"
    if [ -f "/sbin/ldconfig" ]; then
        showInfo "Running ldconfig"
        ldconfig
    fi
elif [ -f "/sbin/ldconfig" ]; then
    showWarning "You may need to run \"sudo ldconfig\""
fi

