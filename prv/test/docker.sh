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

    git clone https://github.com/Kitware/CMake.git ./
    exitOnError "Failed to clone cmake"

    git checkout $CMAKEVER
    exitOnError "Failed to checkout tag : $CMAKEVER"

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
    echo "--- $CMAKEVER"
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

    # ./add.sh cmake -y
    # exitOnError "test-cmake: Failed to add cmake to project"

    cmake . -B ./bld -G "Unix Makefiles"
    exitOnError "test-cmake: Failed to configure"

    cmake --build ./bld
    exitOnError "test-cmake: Failed to build"

    cmake --install ./bld
    exitOnError "test-cmake: Failed to install"

    libmembus test

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

