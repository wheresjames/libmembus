#!/bin/bash

SCRIPTPATH=$(realpath ${BASH_SOURCE[0]})
ROOTDIR=$(dirname $SCRIPTPATH)
if [[ -f "${ROOTDIR}/rbashutils.sh" ]]; then
    . "${ROOTDIR}/rbashutils.sh"
elif [[ -f "${ROOTDIR}/../sh/rbashutils.sh" ]]; then
    . "${ROOTDIR}/../sh/rbashutils.sh"
else
    echo "rbashutils.sh not found"
    exit -1
fi

#--------------------------------------------------------------------------------------------------
# Functions

installCMake()
{
    CMAKEVER=$1

    showInfo "Installing CMake version $CMAKEVER"

    apt-get -yqq update
    apt-get -yqq install libssl-dev

    mkdir -p /code/cmake
    cd /code/cmake

    wget https://github.com/Kitware/CMake/releases/download/v${CMAKEVER}/cmake-${CMAKEVER}.tar.gz
    exitOnError "Failed to download cmake version $CMAKEVER"

    tar xvzf ./cmake-${CMAKEVER}.tar.gz
    exitOnError "Failed to extract cmake version $CMAKEVER"

    cd cmake-${CMAKEVER}
    exitOnError "Failed to cd cmake version $CMAKEVER"

    ./bootstrap
    exitOnError "Failed to bootstrap cmake version $CMAKEVER"

    make -j8
    exitOnError "Failed to build cmake version $CMAKEVER"

    make install
    exitOnError "Failed to install cmake version $CMAKEVER"

    cd /code
    rm -Rf /code/cmake
}


#--------------------------------------------------------------------------------------------------
if [[ $1 == "check-cmake" ]]; then

    CMAKEVER=$(cmake --version)
    echo "$CMAKEVER"
    if compareVersion "$CMAKEVER" "$2" "<"; then
        echo "CMake version is below $2, installing $3"
        installCMake "$3"
    fi

fi


#--------------------------------------------------------------------------------------------------
if [[ $1 == "test-cmake" ]]; then

    showBanner "CMake Test"

    cp -R /code/libmembus /code/libmembus-cmake
    exitOnError "test-cmake: Failed to copy to directory : /code/libmembus-cmake"

    cd /code/libmembus-cmake
    exitOnError "test-cmake: Failed to switch to directory : /code/libmembus-cmake"

    ./clean.sh

    ./add.sh cmake -y
    exitOnError "test-cmake: Failed to add cmake to project"

    cmake . -B ./bld -G "Unix Makefiles"
    exitOnError "test-cmake: Failed to configure"

    cmake --build ./bld
    exitOnError "test-cmake: Failed to build"

    cmake --install ./bld
    exitOnError "test-cmake: Failed to install"

    libmembus uninstall
    exitOnError "test-cmake: Failed to uninstall"

    # --- Install with deb file

    # cpack -B ./pck --config ./bld/CPackConfig.cmake -G DEB -C Release
    # exitOnError "test-conan: Failed to create package"

    # sudo apt install ./pck/libmembus-0.1.0-Linux.deb
    # exitOnError "test-conan: Failed to install deb package"

    # libmembus test
    # exitOnError "test-cmake: Failed to test"

    # apt remove libmembus
    # exitOnError "test-cmake: Failed to uninstall"

fi


#--------------------------------------------------------------------------------------------------
if [[ $1 == "test-conan" ]]; then

    showBanner "Conan Test"

    apt-get -yqq update
    apt-get -yqq install python3 python3-pip
    python3 -m pip install conan

    cp -R /code/libmembus /code/libmembus-conan
    exitOnError "test-conan: Failed to copy to directory : /code/libmembus-conan"

    cd /code/libmembus-conan
    exitOnError "test-conan: Failed to switch to directory : /code/libmembus-conan"

    ./clean.sh

    ./add.sh conan -y
    exitOnError "test-conan: Failed to add conan to project"

    conan install .
    exitOnError "test-conan: Failed to configure"

    conan build .
    exitOnError "test-conan: Failed to build"

    cmake --install ./bld
    exitOnError "test-conan: Failed to install"

    libmembus uninstall
    exitOnError "test-conan: Failed to uninstall"

    # --- Install with deb file

    # conan package .
    # exitOnError "test-conan: Failed to create package"

    # sudo apt install ./pck/libmembus-0.1.0-Linux.deb
    # exitOnError "test-conan: Failed to install deb package"

    # libmembus test
    # exitOnError "test-conan: Failed to test"

    # apt remove libmembus
    # exitOnError "test-cmake: Failed to uninstall"

fi

#--------------------------------------------------------------------------------------------------
if [[ $1 == "test-gradle" ]]; then

    showBanner "Gradle Test"

    apt-get -yqq update
    # apt-get -yqq install gradle

    if [ ! -z $(which gradle) ]; then
        $GRADLEVER=$(gradle --version)
        echo "$GRADLEVER"
    fi
    if compareVersion "$GRADLEVER" "6.3" "<"; then

        GRADLEVER="6.8"

        showInfo "Installing gradle ${GRADLEVER}..."

        apt-get remove gradle
        apt-get -yqq install default-jdk

        TMPDIR=$(mktemp -d -t gradle-XXXXXXXXXX)
        cd "$TMPDIR"
        exitOnError "Faile to create temp directory : $TMPDIR"

        wget -q "https://services.gradle.org/distributions/gradle-${GRADLEVER}-bin.zip" -P "$TMPDIR"
        exitOnError "Failed to download gradle version ${GRADLEVER}"

        unzip -q -d "/opt/gradle" "$TMPDIR/gradle-${GRADLEVER}-bin.zip"
        exitOnError "Failed to extract gradle version ${GRADLEVER}"

        if [ ! -f "/opt/gradle/gradle-${GRADLEVER}/bin/gradle" ]; then
            exitWithError "Failed to install gradle"
        fi

        export GRADLE_HOME="/opt/gradle/gradle-${GRADLEVER}"
        export PATH="${GRADLE_HOME}/bin:${PATH}"

        rm -Rf "$TMPDIR"

    fi

    cp -R /code/libmembus /code/libmembus-gradle
    exitOnError "test-gradle: Failed to copy to directory : /code/libmembus-gradle"

    cd /code/libmembus-gradle
    exitOnError "test-gradle: Failed to switch to directory : /code/libmembus-gradle"

    ./clean.sh

    ./add.sh gradle -y
    exitOnError "test-gradle: Failed to add gradle to project"

    gradle build
    exitOnError "test-gradle: Failed to build"

fi


#--------------------------------------------------------------------------------------------------
if [[ $1 == "test-pip" ]]; then

    showBanner "pip Test"

    apt-get -yqq update
    apt-get -yqq install python3 python3-pip
    exitOnError "test-pip: Failed to install dependencies"

    cp -R /code/libmembus /code/libmembus-pip
    exitOnError "test-pip: Failed to copy to directory : /code/libmembus-pip"

    cd /code/libmembus-pip
    exitOnError "test-pip: Failed to switch to directory : /code/libmembus-pip"

    ./clean.sh

    ./add.sh pip -y
    exitOnError "test-pip: Failed to add pip to project"

    pip3 install .
    exitOnError "test-pip: Failed to install pip module"

    pip3 list

    # export PYTHONPATH=${PYTHONPATH}:/usr/local/

    # python3 ./src/pytest/py/test.py
    # exitOnError "test-pip: Failed to pass test"

    pip3 uninstall -y libmembus
    exitOnError "test-pip: Failed to uninstall"

fi

#--------------------------------------------------------------------------------------------------
if [[ $1 == "test-npm" ]]; then

    showBanner "npm Test"

    apt-get -yqq update
    apt-get -yqq install git nodejs npm
    exitOnError "test-npm: Failed to install dependencies"

    cp -R /code/libmembus /code/libmembus-npm
    exitOnError "test-npm: Failed to copy to directory : /code/libmembus-npm"

    cd /code/libmembus-npm
    exitOnError "test-npm: Failed to switch to directory : /code/libmembus-npm"

    ./clean.sh

    ./add.sh npm -y
    exitOnError "test-npm: Failed to add npm to project"

    cmake . -B ./bld
    exitOnError "test-npm: Failed to configure"

    cmake --build ./bld
    exitOnError "test-npm: Failed to build"

    npm link
    exitOnError "test-npm: Failed to install npm module"

    npm -g list

    export NODE_PATH=/usr/local/lib/node_modules

    node ./src/node-test/js/test.js
    exitOnError "test-npm: Failed to pass test"

    npm -g rm libmembus
    exitOnError "test-npm: Failed to uninstall"

fi



