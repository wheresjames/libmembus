#!/bin/bash

# We're here
IS_RBASHUTILS="YES"

# Root directory
if [[ ! -z "${BASH_SOURCE[0]}" ]]; then
    RBASHUTIL_SCRIPTPATH=$(realpath ${BASH_SOURCE[0]})
    if [[ ! -z "$RBASHUTIL_SCRIPTPATH" ]]; then
        RBASHUTIL_ROOTDIR=$(dirname $RBASHUTIL_SCRIPTPATH)
    else
        RBASHUTIL_ROOTDIR=.
    fi
else
    RBASHUTIL_ROOTDIR=.
fi
if [[ ! -d "$RBASHUTIL_ROOTDIR" ]]; then RBASHUTIL_ROOTDIR="$PWD"; fi
RBASHUTIL_TOOLPATH="${RBASHUTIL_ROOTDIR}/.tools"
RBASHUTIL_ONEXIT=


# Creates a build string based on the current timestamp
# @returns Build string formated as "YY.MM.DD.hhmm"
createBuildString()
{
    local T=(`bash -c "date +'%y %m %d %H %M'"`)

    # Remove leading zeros
    for i in ${!T[@]}; do
        T[$i]=$((10#${T[$i]}))
    done

    echo "${T[0]}.${T[1]}.${T[2]}.$((100 * ${T[3]} + ${T[4]}))"
}

# Pads string to the specified number of characters
# @param [in] string - String to pad
# @param [in] int    - Length to pad string
# @param [in] char   - Padding character
padStr()
{
    local S1="$1"
    local LN=$2
    local PD="$3"
    if [[ -z $PD ]]; then PD=' '; fi
    while ((${#S1} < $LN)); do
        S1+="$PD"
    done
    echo "$S1"
}

# Left pads string to the specified number of characters
# @param [in] string - String to pad
# @param [in] int    - Length to pad string
# @param [in] char   - Padding character
padStrLeft()
{
    local S1="$1"
    local LN=$2
    local PD="$3"
    if [[ -z $PD ]]; then PD=' '; fi
    while ((${#S1} < $LN)); do
        S1="${PD}${S1}"
    done
    echo "$S1"
}

# Limits string length to specified length
# @param [in] string - String to limit
# @param [in] int    - Length to limit string
# @param [in] string - Ending string
limitStr()
{
    local S1="$1"
    local LN=$2
    local END="$3"
    if [[ -z "$END" ]]; then END='...'; fi

    local LIM=$(($LN - ${#END}))
    if [[ ${#S1} -gt $LIM ]]; then
        S1="${S1:0:$LIM}${END}"
    fi
    echo "$S1"
}



# Put a border around a string
# @param [in]    char   - Character to make the box from
# @param [in...] string - String that goes in the box
boxStr()
{
    local BCHR='-'
    local SCHR='|'

    if [[ ${#1} -eq 1 ]]; then
        BCHR=$1
        SCHR=$1
        shift
    elif [[ ${#1} -eq 2 ]]; then
        BCHR=${1:0:1}
        SCHR=${1:1:1}
        shift
    fi

    local STR="${@}"
    local BORDERLEN=$((${#STR}+4))
    local BORDERSTR=$(padStr "$BCHR" $BORDERLEN "$BCHR")

    echo "$BORDERSTR"
    echo -e "$SCHR ${STR} $SCHR"
    echo "$BORDERSTR"
}


# Shows the value of specified script variables
# @param [in]    char   - Border character
# @param [in...] string - Script variable(s)
showVars()
{
    local BCHR='-'
    local SCHR='|'
    local MAXK=30
    local MAXV=80

    if [[ ${#1} -eq 1 ]]; then
        BCHR=$1
        SCHR=$1
        shift
    elif [[ ${#1} -eq 2 ]]; then
        BCHR=${1:0:1}
        SCHR=${1:1:1}
        shift
    fi

    local VARLEN=4
    local VALLEN=4
    local BORDERLEN=1
    for var in "$@"; do
        local VAL=${!var}
        if [[ ${#VAL} -gt $VALLEN ]]; then VALLEN=${#VAL}; fi
        if [[ $MAXV -lt $VALLEN ]]; then VALLEN=$MAXV; fi
        if [[ ${#var} -gt $VARLEN ]]; then VARLEN=${#var}; fi
        if [[ $MAXK -lt $VARLEN ]]; then VARLEN=$MAXK; fi
    done

    local BORDERLEN=$(($VARLEN+$VALLEN+7))
    local BORDERSTR=$(padStr "$BCHR" $BORDERLEN "$BCHR")

    echo "$BORDERSTR"
    for var in "$@"; do
        local S1=$(padStr "$(limitStr "$var" $MAXK)" $VARLEN)
        local S2=$(padStr "$(limitStr "${!var}" $MAXV)" $VALLEN)
        echo "$SCHR $S1 : $S2 $SCHR"
    done
    echo "$BORDERSTR"
}

# Show banner
# @param [in...] string
showBanner()
{
    if [[ 0 -lt ${#@} ]]; then
        local STR="${@}"
        local BORDERLEN=$((${#STR}+10))
        local BORDERSTR=$(padStr - $BORDERLEN -)

        echo
        echo $BORDERSTR
        echo -e "[\e[1;36mINFO\e[1;0m] \e[1;36m${@}\e[1;0m"
        echo $BORDERSTR
        echo
    fi
}

# Show low visibility information
# @param [in...] string
showNotice()
{
    if [[ 0 -lt ${#@} ]]; then
        echo -e "[\e[1;36m\e[1;2mNOTE\e[1;0m] \e[1;36m\e[1;2m${@}\e[1;0m"
    fi
}

# Show information
# @param [in...] string
showInfo()
{
    if [[ 0 -lt ${#@} ]]; then
        echo -e "[\e[1;36mINFO\e[1;0m] \e[1;36m${@}\e[1;0m"
    fi
}

# Show warning
# @param [in...] string
showWarning()
{
    if [[ 0 -lt ${#@} ]]; then
        echo -e "[\e[1;33mWARN\e[1;0m] \e[1;33m${@}\e[1;0m"
    fi
}

# Show Fail
# @param [in...] string
showFail()
{
    if [[ 0 -lt ${#@} ]]; then
        echo -e "[\e[1;31mFAIL\e[1;0m] \e[1;31m${@}\e[1;0m"
    fi
}

# Show error
# @param [in...] string
showError()
{
    if [[ 0 -lt ${#@} ]]; then

        local STR="${@}"
        local BORDERLEN=$((${#STR}+10))
        local BORDERSTR=$(padStr - $BORDERLEN -)

        echo
        echo $BORDERSTR
        echo -e " [\e[1;31mERROR\e[1;0m] \e[1;31m${STR}\e[1;0m"
        echo $BORDERSTR
        echo
    fi
}

# Sets the function to call on exit
# @param [in] function - Function to call on exit
onExit()
{
    RBASHUTIL_ONEXIT=$1
}

# Exits script
# @param [in] int - Exit code
doExit()
{
    if [ ! -z $RBASHUTIL_ONEXIT ]; then
        $RBASHUTIL_ONEXIT $@
    fi

    echo
    exit $1
}

# Show error and exit
# @param [in...] string
exitWithError()
{
    showError $@
    doExit -1
}

# Show error and exit if the last operation did not return 0
# @param [in...] string
exitOnError()
{
    if [[ 0 -eq $? ]]; then return 0; fi
    exitWithError $@
}

# Show error if the last operation did not return 0
# @param [in...] string
showOnError()
{
    if [[ 0 -ne $? ]]; then
        showError $@
    fi
}

# Show string if the last operation returned 0
# @param [in...] string
showOnSuccess()
{
    if [[ 0 -eq $? ]]; then
        showInfo $@
    fi
}

# Show string based on last commands return value
# @param [in] string - String to show on success
# @param [in] string - String to show on error
showStatus()
{
    if [[ 0 -eq $? ]]; then
        showInfo $1
    else
        showError $2
    fi
}

# Execute command if the last operation did not return 0
# @param [in...] string
doIfError()
{
    if [[ 0 -ne $? ]]; then
       $@
    fi
}

# Execute command if the last operation returned 0
# @param [in...] string
doIfSuccess()
{
    if [[ 0 -eq $? ]]; then
        $@
    fi
}

# Show warning if the last operation did not return 0
# @param [in...] string
warnOnError()
{
    if [[ 0 -eq $? ]]; then return 0; fi
    showWarning $@
    return -1
}

# Sets the command list
# @param [in] string - Command list
#
# @example
#   setCmd "build-package-upload"
#   setCmd "build,package,upload"
setCmd()
{
    RBASHUTIL_COMMANDLIST=$1
}

# Returns the command list
getCmds()
{
    echo "$RBASHUTIL_COMMANDLIST"
}

# Checks the variable *$COMMANDLIST* for the specified command
# @param [in] string - Command to search for
#
# @example
#
# if isCmd "upload"; then
#   ... do upload ...
# fi
#
isCmd()
{
    # - separator
    local FOUND=$(echo "\-$RBASHUTIL_COMMANDLIST-" | grep -o "\-${1}-")
    if [[ ! -z $FOUND ]]; then return 0; fi

    # , separator
    local FOUND=$(echo ",$RBASHUTIL_COMMANDLIST," | grep -o ",${1},")
    if [[ ! -z $FOUND ]]; then return 0; fi
    return -1
}

# Deletes the specified command from *$COMMANDLIST*
# @param [in] string - Command to delete
# @todo Have this clean up the separators
delCmd()
{
    if isCmd $1; then
        RBASHUTIL_COMMANDLIST=${RBASHUTIL_COMMANDLIST/${1}/}
    fi
}

# Searches the specified string for a sub string
# @param [in] string - String to search
# @param [in] string - Sub string to look for
# @returns 0 if sub string is found
findInStr()
{
    local FIND_LIST="$1"
    local FIND_EXISTS=$(echo "$FIND_LIST" | grep -o "$2")
    if [[ ! -z $FIND_EXISTS ]]; then return 0; fi
    return -1
}

# Executes a command and searches for a sub string in the output
# @param [in] string - Command to execute (must not contain %)
# @param [in] string - Sub string to search for
# @returns 0 if sub string is found
findIn()
{
    local CMD=
    local RESULT=
    local CMDLIST=(${1// /%})
    local CMDLIST=(${CMDLIST//|/ })
    for CMD in "${CMDLIST[@]}";do
        CMD="${CMD//%/ }"
        RESULT=$($CMD <<< "$RESULT")
    done
    findInStr "$RESULT" $2
    return $?

    # Doesn't handle pipes
    # findInStr "$($1 2>&1)" $2
    # return $?
}

# Searches a multi line string for a line matching the specfied regex
# @param [in] string    - Multi line string to search
# @param [in] regex     - Regular expression
# @param [in] max       - Number of lines to return
# @returns The first line that matches
filterLines()
{
    local STR=$1
    local RGX=$2
    local MAX=$3

    local LINE=
    local RES=
    while read -r LINE; do
        if [[ $LINE =~ $RGX ]]; then
            RES="$RES\n$LINE"
            MAX=$(($MAX - 1))
            if [[ 0 -ge $MAX ]]; then
                break
            fi
        fi
    done <<< "$STR"
    echo "$RES"
}


# Checks if the specified command is an executable program
# @param [in] string - Command to search for
# @returns 0 if valid command
isCommand()
{
    if ! findIn "which $1" "$1"; then return -1; fi
    return 0
}

# Checks if the specified package is installed
isAptPkgInstalled()
{
    if findIn "dpkg --get-selections" $1; then return 0; else return -1; fi
}

# Checks that the specified pacakge(s) is installed, installs if not
# @param [in] string - List of packages
# @notes Exits on failure
aptInstall()
{
    local QUIET=
    if [[ "-q" == "$1" ]]; then
        QUIET="YES"
        shift
    fi

    for PKG in "$@";do
        # if findIn "dpkg --get-selections" $PKG; then
        if isAptPkgInstalled "$PKG"; then
            if [ -z "$QUIET" ]; then
                showNotice "[x] Already installed: $PKG"
            fi
        else
            showInfo "Installing $PKG..."
            apt-get -yq install $PKG
            exitOnError "Failed to install $PKG"
        fi
    done
}

# Checks for the specified apt repository
# @param [in] string - Repository to search for
# @returns 0 if found
isAptRepo()
{
    if ! findIn "egrep -v '^#|^ *$' /etc/apt/sources.list /etc/apt/sources.list.d/*" "$1"; then return -1; fi
    return 0
}

# Adds the specified apt repository if not already in sources.list
# @param [in] string - Repository to add
# @returns 0 if already exists or added
addAptRepo()
{
    if ! isAptRepo "$1"; then
        apt-add-repository $1
        if ! warnOnError "Unable to add repo $1"; then
            apt-get -yq update
        fi
    fi
}

# Executes a command and if the specified sub string is found,
# executes a second command, may optionally exit with error
# @param [in] string - Command to execute
# @param [in] string - Sub string to find in command output
# @param [in] string - Command to execute if sub string is found
# @param [in] string - [optional] If set, application will exit
#                                 with this error message if found
doIf()
{
    findInStr "$($1 2>&1)" $2
    if [[ 0 -ne $? ]]; then return 0; fi
    $3
    if [[ ! -z "$4" ]]; then
        exitWithError $4
    fi
}

# Executes a command and if the specified sub string is *not* found,
# executes a second command, may optionally exit with error
# @param [in] string - Command to execute
# @param [in] string - Sub string to find in command output
# @param [in] string - Command to execute if sub string is *not* found
# @param [in] string - [optional] If set, application will exit
#                                 with this error message if found
doIfNot()
{
    findInStr "$($1 2>&1)" $2
    if [[ 0 -eq $? ]]; then return 0; fi
    $3
    if [[ ! -z "$4" ]]; then
        exitWithError $4
    fi
}

# Executes a second command if the first command fails
# @param [in]       - First command to execute
# @param [in]       - Second command to execute if the first fails
# @param [in,opt]   - Message to display on error
doIfFail()
{
    local E=$?
    if [ ! -z "$1" ]; then
        $1
        E=$?
    fi
    if [[ 0 -ne $E ]]; then
        if [ ! -z "$3" ]; then
            echo "$3"
        fi
        $2
    fi
}


# Executes a second command if the first command fails and exits
# @param [in]       - First command to execute
# @param [in]       - Second command to execute if the first fails
# @param [in,opt]   - Message to display on error
doIfFailAndExit()
{
    local E=$?
    if [ ! -z "$1" ]; then
        $1
        E=$?
    fi
    if [[ 0 -ne $E ]]; then
        $2
        exitWithError $3
    fi
}


# Waits while a sub string appears in a commands output
# @param [in] string - Command to execute
# @param [in] string - String to search for
# @param [in] string - Prompt if wait is needed
# @param [in] int    - [optional] Maxium number of retries
#                                 0 = forever (999999)
# @param [in] int    - [optional] Seconds to delay between checks
#                                 Default = 3
waitWhile()
{
    if ! findIn "$1" "$2"; then return 0; fi

    local WAITRETRY=$4
    if [[ -z $WAITRETRY ]]; then WAITRETRY=999999; fi

    local DELAYTIME=$5
    if [[ -z $DELAYTIME ]]; then DELAYTIME=3; fi

    echo "$3 "
    while findIn "$1" "$2"; do

        # Retries?
        WAITRETRY=$((WAITRETRY-1))
        if [ $WAITRETRY -le 0 ]; then
            return -1
        fi

        printf .
        sleep $DELAYTIME
    done

    echo
    echo "Done"
    return 0
}

# Waits until a sub string appears in a commands output
# @param [in] string - Command to execute
# @param [in] string - String to search for
# @param [in] string - Prompt if wait is needed
# @param [in] int    - [optional] Maxium number of retries
#                                 0 = forever (999999)
# @param [in] int    - [optional] Seconds to delay between checks
#                                 Default = 3
waitUntil()
{
    if findIn "$1" "$2"; then return 0; fi

    local WAITRETRY=$4
    if [[ -z $WAITRETRY ]]; then WAITRETRY=999999; fi

    local DELAYTIME=$5
    if [[ -z $DELAYTIME ]]; then DELAYTIME=3; fi

    echo "$3 "
    while ! findIn "$1" "$2"; do

        # Retries?
        WAITRETRY=$((WAITRETRY-1))
        if [ $WAITRETRY -le 0 ]; then
            return -1
        fi

        printf .
        sleep $DELAYTIME
    done

    echo
    echo "Done"
    return 0
}

# Ask user a yes or no question
# @param [in] string    - Question
# @returns Zero if user answers yes, or -1
askYesNo()
{
    local yn=
    while [[ -z $yn ]]; do
        read -p "$* [y/n]: " yn
        case $yn in
            [Yy]*) return 0  ;;
            [Nn]*) return -1 ;;
            *) yn=
        esac
    done
}

# Returns 0 if the specified link is available
# @param [in] string - Link to check
# @example
#   isOnline "http://google.com"
isOnline()
{
    wget -q --tries=1 --timeout=8 --spider $1
    return $?
}

# Adds the specified environment variable to the specified file
# if it is not already found in the file, otherwise, modifies the
# existing variable
#
# @param [in] string - Environment variable name
# @param [in] string - Environment variable value
# @param [in] string - File in which to add variable
# @example
#   setEnv "MYVARIABLE" "MYVALUE" "/root/.bashrc"
setEnv()
{
    local VAR=$1
    local VAL=$2
    local FILE=$3

    if ! findIn "cat ${FILE}" "export ${VAR}="; then
        printf "\nexport ${VAR}=${VAL}\n" >> "${FILE}"
    else
        sed -i "s/export ${VAR}=.*/export ${VAR}=${VAL}/g" "${FILE}"
    fi
}

# Checks if the string contains spaces
# @param [in] string - String to check for spaces
# @returns non-zero if there are spaces else zero
containsSpaces()
{
	if [[ "$1" != "${1/ /}" ]]; then return 0; fi
	return 1
}

# Finds the first file matching the specified template
# @param [in] string - Root path to search
# @param [in] string - File template
# @example
#   findFile "/etc" "hos.*"
findFile()
{
    local SEARCHROOT=$1
    if [ ! -d $SEARCHROOT ]; then
        exitWithError "findFile search path not specified or valid"
    fi

    local FINDTMPL=$2
    if [ -z $FINDTMPL ]; then
        exitWithError "findFile template not specified"
    fi

    # Search for a file matching the template?
    local FINDFILE=$(find $SEARCHROOT | grep "$FINDTMPL" | head -1)

    # Make sure file exists
    if [ ! -f "$FINDFILE" ]; then
        FINDFILE=
    fi

    echo "$FINDFILE"
}

# Searches current and parent directories for the specified file
# @param [in] string    - Directory to start in
# @param [in] string    - File name to search for
# @returns The first path containing the file or empty string
findParentWithFile()
{
    local DIR=$1
    local FILE=$2

    # Find project folder
    local SEARCH="$DIR"
    while [[ ! -f "${SEARCH}/${FILE}" ]] && [[ 1 -lt ${#SEARCH} ]]; do
        SEARCH=$(dirname $SEARCH)
    done

    if [[ ! -f "${SEARCH}/${FILE}" ]]; then
        SEARCH=
    fi

    echo "${SEARCH}"
}


# Creates a random password
# @param [in] int    - Password length
# @param [in] string - Password name
# @param [in] string - Password locations (default ./secrets)
# @param [in] string - Character set, default = "A-Za-z0-9"
#
# @example
#
#   MYPASSWORD=$(getPassword 16 "mypassword")
#
#   MYOTHERPASSWORD=$(getPassword 32 "mypassword" "" "A-Za-z0-9!@#$%&*")
#
#   DONTSAVE=$(getPassword 16 "" "" "A-Za-z0-9!@#$%&*")
#
getPassword()
{
    local PWDLEN=$1
    local PWDNAME=$2
    local PWDPATH=$3
    local CHARSET=$4

    if [[ -z $PWDLEN ]]; then return -1; fi
    if [[ -z $CHARSET ]]; then CHARSET="A-Za-z0-9"; fi

    local PASSWORD=
    local PWDFILE=
    if [[ ! -z $PWDNAME ]]; then

        if [[ -z $PWDPATH ]]; then
            PWDPATH="./secrets"
        fi

        if [[ ! -d "$PWDPATH" ]]; then
            mkdir -p "$PWDPATH"
        fi

        PWDFILE="${PWDPATH}/$PWDNAME.pwd"
        if [[ ! -z $NEWPASSWORD ]]; then
            rm "$PWDFILE"
        elif [[ -f $PWDFILE ]]; then
            PASSWORD=$(<${PWDFILE})
        fi
    fi

    if [[ -z $PASSWORD ]]; then
        PASSWORD=$(head /dev/urandom | tr -dc "$CHARSET" | head -c ${PWDLEN})
        if [[ ! -z $PWDFILE ]]; then
            echo "${PASSWORD}" > "${PWDFILE}"
        fi
    fi

    echo "$PASSWORD"
}

# Returns wifi devices and status
getWifi()
{
    echo $(nmcli -t -f active,ssid dev wifi)
}

# Returns the most recent modified timestamp in a directory tree
# @param [in] string    - directory name
lastModified()
{
    local FILES="$1/*"

    TS=0
    for SRC in $FILES
    do
        if [[ -d "$SRC" ]]; then

            local TTS=$(lastModified "$SRC")
            if [[ $TS < $TTS ]]; then
                TS=$TTS
            fi

        # Is the directory empty?
        elif [[ "$SRC" =~ "*" ]]; then

            # Empty
            EMPTY=

        # Process this file
        elif [[ -f "$SRC" ]]; then
            local TTS=$(date +%s -r "${SRC}")
            if [[ $TS < $TTS ]]; then
                TS=$TTS
            fi
        fi

    done

    echo "$TS"
}


# Compares two version numbers
# @param [in] string    - First version number
# @param [in] string    - Second version number
# @param [in] string    - Compare operator [">", ">=", "=", "<", "<=", "><"]
#                           =       - Is equal to
#                           >       - Greater than
#                           <       - Less than
#                           >=      - Greater than or equal to
#                           <=      - Less than or equal to
#                           ><      - Not Equal to
# @param [in] string    - Options
#                           show    - Show comparison equation result
#
# @returns Non-zero if the comparison is false
#
# This function will parse all numbers from the given version strings.
# Comparison will stop once there is not a corrisponding number in each string.
#
# Example
# @code
#
# if compareVersion "$(cmake --version)" "3.15" "<"; then
#     echo "Version is too low"
# fi
#
# @endcode
#
compareVersion()
{
    # Get input params
    local V1="$1"
    local V2="$2"
    local CP="$3"
    local AC="$4"

    # Find the first line with numbers in it
    V1=$(filterLines "$V1" [0-9] 1)
    V2=$(filterLines "$V2" [0-9] 1)

    # Parse into array
    IFS=' ' read -r -a V1 <<< "${V1//[^0-9]/ }"
    IFS=' ' read -r -a V2 <<< "${V2//[^0-9]/ }"

    local IDX=0
    local MAX=${#V1[@]}
    if [[ $MAX -lt ${#V2[@]} ]]; then MAX=${#V2[@]}; fi

    RES=
    while [[ -z $RES ]] && [[ $IDX -lt $MAX ]]; do

        local NEXT=$(($IDX + 1))

        local a=0
        if [ $IDX -lt ${#V1[@]} ]; then a=${V1[$IDX]}; fi

        local b=0
        if [ $IDX -lt ${#V2[@]} ]; then b=${V2[$IDX]}; fi

        if [[ $a -gt $b ]]; then RES=">"; fi
        if [[ $a -lt $b ]]; then RES="<"; fi

        # Last number?
        if [[ $NEXT -ge $MAX ]]; then
            if [[ $a -eq $b ]]; then RES="="; fi
        fi

        IDX=$NEXT

    done

    # Empty is equal
    if [[ -z $RES ]]; then RES="="; fi

    # Show results
    if [ "show" == "$AC" ]; then
        V1=${V1[@]}
        V2=${V2[@]}
        echo "'${V1// /.}' $RES '${V2// /.}'"

    # Print comparison
    elif [ -z $CP ]; then
        printf "$RES"
    fi

    # Return failure if doesn't match users parameter
    if [[ ! -z "$CP" ]] && [[ "$CP" != *"$RES"* ]]; then
        return -1;
    fi

    return 0
}


# Compares two version numbers and exits if they don't match expected result
# @param [in] string    - First version number
# @param [in] string    - Second version number
# @param [in] string    - Compare operator [">", ">=", "=", "<", "<="]
assertVersion()
{
    if ! compareVersion "$1" "$2" "$3" "show"; then
        exitWithError "'$1' $3 '$2'"
    fi
}

# Recursively counts the files in the specified directory
# @param [in] string    - Directory containing files to count
# @param [in] string    - Non-zero to count directories as well
# @returns Number of files in specified directory
countFiles()
{
    local DIR=$1
    local CNTDIRS=$2
    if [ ! -d "$DIR" ]; then
        echo "0"
        return 0
    fi

    local COUNT=0
    local FILES=$DIR/*
    for SRC in $FILES
    do
        # Empty
        if [[ $SRC =~ "*" ]]; then
            continue
        fi

        #Directory
        if [[ -d "$SRC" ]]; then
            MORE=$(countFiles "$SRC")
            if [ ! -z "$CNTDIRS" ]; then
                COUNT=$(($COUNT+$MORE+1))
            else
                COUNT=$(($COUNT+$MORE+1))
            fi

        # File
        else
            COUNT=$(($COUNT+1))
        fi

    done

    echo "$COUNT"
}

# Iterate files in a given directory
# @param [in] func   - Function to call for each file / directory
# @param [in] string - Directory to iterate
# @param [internal]  - Total number of files in directory
# @param [internal]  - Current file count
iterateFiles()
{
    local FN=$1
    local DIR=$2
    local TOT=$3
    local CNT=$4

    if [ ! -d "$DIR" ]; then
        return -1
    fi

    if [ -z "$TOT" ]; then
        TOT=$(countFiles "$DIR" YES)
        CNT="RBASHUTILS_ITTRCOUNT_$(getPassword 8)"
        declare -g $CNT=0
    fi

    local FILES=$DIR/*
    for SRC in $FILES
    do
        # Empty
        if [[ $SRC =~ "*" ]]; then
            continue
        fi

        local PROG="[ -- ]"
        declare -g $CNT=$((${!CNT}+1))
        if [[ 0 -lt $TOT ]]; then
            local PERCENT=$((${!CNT} * 100 / $TOT))
            if [[ 0 -gt $PERCENT ]]; then PERCENT=0;
            elif [[ 100 -lt $PERCENT ]]; then PERCENT=100; fi
            PROG="[$(padStrLeft "$PERCENT" 3)%]"
        fi

        # Directory
        if [[ -d "$SRC" ]]; then
            $FN "$SRC" "$PROG"
            iterateFiles $FN "$SRC" $TOT $CNT

        # Files
        else
            $FN "$SRC" "$PROG"
        fi
    done
}

# Returns 0 if the cert is valid for the specified number of days
# @param [in] string    - Domain name to check
# @param [in] int       - Number of days (default is 1)
isCertValid()
{
    local DOMAIN=$1
    local DAYS=$2

    if [[ -z "$DAYS" ]] || [[ 0 -eq $DAYS ]]; then
        DAYS=1
    fi
    local SECS=$(($DAYS * 24 * 60 * 60))

    CERTRAW=$(openssl s_client -connect ${DOMAIN}:443 -servername ${DOMAIN} 2>/dev/null </dev/null)
    if [ -z "$CERTRAW" ]; then
        return -1;
    fi

    CERTTXT=$(echo "${CERTRAW}" | openssl x509 -in /dev/stdin -noout -checkend $SECS)
    if findInStr "$CERTTXT" "Certificate will not expire"; then
        return 0;
    fi

    return -1;
}


# Returns issue / expire info from certificate
# @param [in] string - Domain name
# @param [in] option - Which time (can be both) [start, end]
# @param [in] option - Format [text, timestamp]
getCertTime()
{
    local DOMAIN=$1
    local WHICH=$2
    local FORMAT=$3
    local RET=

    local CERTRAW=$(openssl s_client -connect ${DOMAIN}:443 -servername ${DOMAIN} 2>/dev/null </dev/null)
    if [ -z "$CERTRAW" ]; then
        return -1;
    fi

    # Start time
    if findInStr "$WHICH" "start"; then
        local CERTTXT=$(echo "${CERTRAW}" | openssl x509 -in /dev/stdin -noout -startdate)
        if findInStr "$CERTTXT" "notBefore="; then
            if [[ "timestamp" == "$FORMAT" ]]; then
                RET="$(date --date="${CERTTXT:10}" +"%s")"
            else
                RET="${CERTTXT:10}"
            fi
        fi
    fi

    # End time
    if findInStr "$WHICH" "end"; then
        local CERTTXT=$(echo "${CERTRAW}" | openssl x509 -in /dev/stdin -noout -enddate)
        if findInStr "$CERTTXT" "notAfter="; then
            if [ ! -z "$RET" ]; then
                RET="$RET : "
            fi
            if [[ "timestamp" == "$FORMAT" ]]; then
                RET="${RET}$(date --date="${CERTTXT:9}" +"%s")"
            else
                RET="${RET}${CERTTXT:9}"
            fi
        fi
    fi

    echo "${RET}"
}

