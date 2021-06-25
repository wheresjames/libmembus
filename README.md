
# libmembus


---------------------------------------------------------------------
## Table of contents

* [Quick Start](#quick-start)
* [Using in your project](#using-in-your-project)
* [Administration](#administration)
* [Examples](#examples)
* [References](#references)

&nbsp;


---------------------------------------------------------------------
## Quick Start

### Linux

#### Install Conan and dependencies on debian

    $ sudo apt-get -yq update
    $ sudo apt-get -yq install build-essential git cmake doxygen graphviz go-md2man
    $ sudo apt-get -yq install python3 python3-pip
    $ python3 -m pip install conan


#### Build and install

    $ conan install .
    $ conan build .
    $ sudo cmake --install ./bld


### Cleanup build files

    $ ./clean.sh


### Windows

- Install [CMake](https://cmake.org/download/)
- Install [Visual Studio](https://visualstudio.microsoft.com/downloads/).
  *The free **Community Edition** is fine*
- Install [WSL for Windows](https://docs.microsoft.com/en-us/windows/wsl/install-win10)

#### Build and install

From the [Windows Powershell](https://docs.microsoft.com/en-us/powershell/)

    > conan install . -s build_type=Release
    > conan build .

The following command must be run as an Administrator

    > cmake --install ./bld

&nbsp;


---------------------------------------------------------------------
## Using in your project

To Include this library in your C/C++ file.

    #include <libmembus.h>

You also need to add the linker option `-lmembus`

If you prefer, you can use pkg-config

    pkg-config --cflags libmembus
    pkg-config --libs libmembus


### CMake

    find_package(libmembus)
    if(NOT libmembus_FOUND)
        message(FATAL_ERROR "libmembus not found")
    endif()
    message(STATUS "Found libmembus ${libmembus_VERSION}")
    include_directories("${libmembus_INCLUDE_DIRS}")
    link_directories("${libmembus_LIBRARIES}")

    target_link_libraries(${TARGET} PRIVATE libmembus)


&nbsp;


---------------------------------------------------------------------
## Administration (Linux only)

Library commands, for once you have it installed

- Display help

    **$ libmembus help**

- Uninstall

    **$ sudo libmembus uninstall**

- Show installation roots

    **$ libmembus files**

- Get installation information.  (Change this info in **PROJECT.txt**)

    **$ libmembus info \<variable\>**

    variable = One of [name, description, url, version, build, company, author, lib, include, bin, share]

&nbsp;


---------------------------------------------------------------------
## Examples

### Sending messages

    mmb::memmsg tx, rx;
    std::string msg = "Message";

    tx.open("/mymsg", 1024, true, true);
    rx.open("/mymsg", 1024, false, false);

    tx.write(msg);
    std::string rmsg = rx.read(0);
    assertTrue(rmsg == msg)

&nbsp;


---------------------------------------------------------------------
## References

- CMake
    - https://cmake.org

- Conan
    - https://conan.io

