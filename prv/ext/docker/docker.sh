#!/bin/bash

#--------------------------------------------------------------------------------------------------
if [[ "init" == $1 ]]; then

    # Build dependencies
    apt-get -yq update
    apt-get -yq install wget build-essential cmake doxygen python3 python3-pip

    # Install conan
    python3 -m pip install conan

    # Install a web browser to view help file (libblank help)
    apt-get -yq install lynx

fi


#--------------------------------------------------------------------------------------------------
if [[ $1 == "cmake" ]]; then

    apt-get -yq update
    apt-get -yq install libssl-dev

    mkdir -p /code/cmake
    cd /code/cmake

    wget https://github.com/Kitware/CMake/releases/download/v${2}/cmake-${2}.tar.gz
    tar xvzf ./cmake-${2}.tar.gz
    cd cmake-${2}

    ./bootstrap
    make
    make install

    cd /code
    rm -Rf /code/cmake

fi

