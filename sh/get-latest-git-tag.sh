#!/usr/bin/env bash

get_latest_git_tag()
{
    local GITURL=$1
    local FILTER=${2:-".*"}  # Default: match all tags

    if [ -z "$GITURL" ]; then
        return 1
    fi

    all_tags=$(git ls-remote --tags "$GITURL" | \
        grep -o 'refs/tags/.*$' | \
        sed 's|refs/tags/||' | \
        grep -v '\^{}' | \
        grep -E "$FILTER")

    if [ -z "$all_tags" ]; then
        return 1
    fi

    latest_tag=$(echo "$all_tags" | \
        awk '
        {
            orig = $0
            if (match(orig, /[0-9]+(\.[0-9]+){0,2}/)) {
                ver = substr(orig, RSTART, RLENGTH)
                n = split(ver, parts, ".")
                major = parts[1]
                minor = (n >= 2 ? parts[2] : 0)
                patch = (n == 3 ? parts[3] : 0)
                printf "%d.%d.%d|%s\n", major, minor, patch, orig
            }
        }' | \
        sort -t. -k1,1n -k2,2n -k3,3n | \
        tail -n1 | \
        cut -d'|' -f2)

    if [ -z "$latest_tag" ]; then
        return 1
    fi

    echo "$latest_tag"
}


# If this script is called with a git URL
GITURL=$1
FILTER=$2
if [ ! -z "${GITURL}" ]; then
    echo $(get_latest_git_tag ${GITURL} ${FILTER})
fi
