#!/bin/bash

if [ -f "${SHDIR}/MANIFEST.txt" ]; then
    cat "${SHDIR}/MANIFEST.txt"

elif [ -f "${ROOTDIR}/bld/MANIFEST.txt" ]; then
    cat "${ROOTDIR}/bld/MANIFEST.txt"

elif [ -f "${ROOTDIR}/bld/install_manifest.txt" ]; then
    cat "${ROOTDIR}/bld/install_manifest.txt"

fi

echo
