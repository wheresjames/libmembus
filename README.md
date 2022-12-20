
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
    $ sudo apt-get -yq install build-essential git cmake libboost-all-dev doxygen graphviz go-md2man


#### Build and install

    $ cmake . -B ./bld
    $ cmake --build ./bld -j4
    $ sudo cmake --install ./bld

 OR using conan

    $ conan install .
    $ conan build .
    $ sudo cmake --install ./bld


### Cleanup build files

    $ ./clean.sh


### Windows

- Install [CMake](https://cmake.org/download/)
- Install [git](https://git-scm.com/downloads)
- Install [boost](https://sourceforge.net/projects/boost/files/boost-binaries/)
- Install [Visual Studio](https://visualstudio.microsoft.com/downloads/). *The free **Community Edition** is fine*
- Install [WSL for Windows](https://docs.microsoft.com/en-us/windows/wsl/install-win10)

#### Build and install

From the [Windows Powershell](https://docs.microsoft.com/en-us/powershell/)

    > cmake . -B ./bld
    > cmake --build ./bld -j4

OR with conan

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

If it's installed

    find_package(libmembus)
    if(NOT libmembus_FOUND)
        message(FATAL_ERROR "libmembus not found")
    endif()
    message(STATUS "Found libmembus ${libmembus_VERSION}")
    include_directories("${libmembus_INCLUDE_DIRS}")
    link_directories("${libmembus_LIBRARIES}")

    target_link_libraries(<TARGET> PRIVATE libmembus)

OR use FetchContent

    FetchContent_Declare(
        libmembus
        GIT_REPOSITORY      https://github.com/wheresjames/libmembus.git
        GIT_TAG             v0.1.3
        GIT_PROGRESS        TRUE
    )
    FetchContent_MakeAvailable(libmembus)
    include_directories(${libmembus_SOURCE_DIR}/src/main/headers)
    link_directories(${libmembus_BINARY_DIR}/lib)

    add_dependencies(<TARGET> libmembus)
    target_link_libraries(<TARGET> PRIVATE $<$<PLATFORM_ID:Windows>:lib>membus_static)

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

``` C++

    mmb::memmsg tx, rx;
    std::string msg = "Message";

    tx.open("/mymsg", 1024, true, true);
    rx.open("/mymsg", 1024, false, false);

    tx.write(msg);
    std::string rmsg = rx.read(0);
    assertTrue(rmsg == msg)

```

&nbsp;


---------------------------------------------------------------------
## References

- CMake
    - https://cmake.org

- Conan
    - https://conan.io

- git
    - https://git-scm.com/

- Boost
    - https://www.boost.org/

- Visual Studio
    - https://visualstudio.microsoft.com/

- Doxygen
    - https://github.com/doxygen/doxygen

- Graphviz
    - https://graphviz.org/

- Md2man
    - https://sunaku.github.io/md2man/man/man5/md2man.5.html
    - https://github.com/sunaku/md2man
