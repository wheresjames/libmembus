#====================================================================
#
#   Configure the build
# $ cmake . -B ./bld  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#
#   Execute build
# $ cmake --build ./bld -j 8
#
#   Install/Uninstall library
# $ sudo cmake --install ./bld && sudo ./run.sh uninstall
#
#   Clean up build files (uninstall does not work after clean)
# $ ./clean.sh
#
#   Create deb package
# $ cpack -B ./pck -G DEB -C Release --config ./bld/CPackConfig.cmake
#
#   Install/uninstall deb package
# $ sudo dpkg -i ./pck/libmembus-0.1-Linux.deb && sudo dpkg -r libmembus
#   or
# $ sudo apt install ./pck/libmembus-0.1-Linux.deb && sudo apt remove libmembus
#
#   Get compiler opts
# $ pkg-config --cflags libmembus
#
#   Get linker opts
# $ pkg-config --libs libmembus
#
#====================================================================


#====================================================================
# Minimum version
cmake_minimum_required(VERSION 3.15)

# Don't allow in builds in the source tree
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed, try cmake --build ./bld")
endif()


#====================================================================
# Sub directories to build
set(SUBDIRS "")
file(GLOB srcdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
foreach(sub ${srcdirs})
    if(${sub} MATCHES "^[^_]")
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/${sub}")
            list(APPEND SUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}/src/${sub}")
        endif()
    endif()
endforeach()


#====================================================================
# Options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(UNIX)
    set(BUILD_SHARED_LIBS ON)
endif()


#====================================================================
# Configure
file(READ "PROJECT.txt" CFGSTR)
string(REPLACE "\r" "\n" CFGSTR ${CFGSTR})
set(CFGSTR "\n${CFGSTR}")

string(REGEX MATCH "[\n]name[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPNAME)

string(REGEX MATCH "[\n]version[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPVER)

string(REGEX MATCH "([^.]*)" _ ${APPVER})
string(STRIP ${CMAKE_MATCH_1} APPMAJ)

string(REGEX MATCH "[^.]*\.([^.]*)" _ ${APPVER})
string(STRIP ${CMAKE_MATCH_1} APPMIN)

string(REGEX MATCH "[\n]description[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPDESC)

string(REGEX MATCH "[\n]company[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPCOMPANY)

string(REGEX MATCH "[\n]author[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPAUTHOR)

string(REGEX MATCH "[\n]email[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPEMAIL)

string(REGEX MATCH "[\n]url[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPURL)

string(REGEX MATCH "[\n]license[ \t]*([^\n]*)" _ ${CFGSTR})
string(STRIP ${CMAKE_MATCH_1} APPLICENSE)

if("${APPBUILD}" STREQUAL "")
    string(TIMESTAMP APPBUILD "%y.%m.%d.%H%M")
    string(REPLACE ".0" "." APPBUILD ${APPBUILD})
endif()


#====================================================================
# Project info
project("${APPNAME}" VERSION "${APPVER}" DESCRIPTION "${APPDESC}" HOMEPAGE_URL "${APPURL}")
if(NOT IS_APPDEFS)
    set(IS_APPDEFS YES)
    add_compile_options(-DAPPNAME="${PROJECT_NAME}")
    add_compile_options(-DAPPNAMERAW=${PROJECT_NAME})
    add_compile_options(-DAPPVER="${PROJECT_VERSION}")
    add_compile_options(-DAPPBUILD="${APPBUILD}")
    add_compile_options(-DAPPDESC="${APPDESC}")
    # add_compile_options(-DAPPCOMPANY="${APPCOMPANY}")
    # add_compile_options(-DAPPAUTHOR="${APPAUTHOR}")
    # add_compile_options(-DAPPEMAIL="${APPEMAIL}")
    # add_compile_options(-DAPPURL="${APPURL}")
endif()


#====================================================================
# Output paths
set(CMAKE_INSTALL_BINDIR bin)
set(CMAKE_INSTALL_SBINDIR sbin)
set(CMAKE_INSTALL_LIBEXECDIR bin)
set(CMAKE_INSTALL_LIBDIR lib)
set(CMAKE_INSTALL_INCLUDEDIR include)
set(CMAKE_INSTALL_OLDINCLUDEDIR include)
set(CMAKE_INSTALL_DATAROOTDIR share)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)


#====================================================================
# Setup Conan
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
endif()


#====================================================================
# Just a really useful debugging function
# https://stackoverflow.com/questions/9298278
#
# Example dump_cmake_variables("optional-search-term")
#
function(dump_cmake_variables)
    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        if (ARGV0)
            unset(MATCHED)
            string(REGEX MATCH ${ARGV0} MATCHED "${_variableName} ${${_variableName}}")
            if (NOT MATCHED)
                continue()
            endif()
        endif()
        message(STATUS " >>>>>>> ${_variableName}=${${_variableName}}")
    endforeach()
endfunction()


#====================================================================
# Info
message(STATUS "-------------------------------------------------------------------------")
message(STATUS "${APPNAME} - ${APPVER} (${APPBUILD})")
message(STATUS "-------------------------------------------------------------------------")
message(STATUS "CMAKE_INSTALL_PREFIX     : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_SOURCE_DIR         : ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR         : ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR : ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR : ${CMAKE_CURRENT_BINARY_DIR}")

execute_process(COMMAND git describe --tags --abbrev=0 OUTPUT_VARIABLE GIT_LASTTAG)
string(STRIP "${GIT_LASTTAG}" GIT_LASTTAG)
if("v${APPVER}" STREQUAL "${GIT_LASTTAG}")
    message(STATUS "######################## : git tag matches version: ${GIT_LASTTAG} == ${APPVER}")
else()
    message(AUTHOR_WARNING "######################### : git tag does not match version: ${GIT_LASTTAG} != ${APPVER}")
endif()
message(STATUS "-------------------------------------------------------------------------")


#====================================================================
# Add sub directories
foreach(ADDDIR ${SUBDIRS})
    if (EXISTS "${ADDDIR}/CMakeLists.txt")
        message(STATUS "Adding directory ${ADDDIR}")
        add_subdirectory("${ADDDIR}")
    else()
        message(STATUS "Ignoring directory ${ADDDIR}")
    endif()
endforeach()


#====================================================================
# Doxygen
find_package(Doxygen)
if (DOXYGEN_FOUND)
    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/prv/dox/doxygen.cfg)
    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${doxyfile_in} ${doxyfile} @ONLY)
    add_custom_target(${APPNAME}-doc ALL
        ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/" DESTINATION share/${PROJECT_NAME}/dox COMPONENT help)
endif()


#====================================================================
# man page
if(UNIX AND NOT APPLE)

    # Check for md2man (md2man-roff ./README.md > README.1)
    if (EXISTS /usr/local/bin/md2man-roff)
        message(STATUS "Generating man page with md2man")
        add_custom_target(man ALL /usr/local/bin/md2man-roff "${CMAKE_CURRENT_SOURCE_DIR}/README.md" > "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1" DESTINATION man/man1)

    # Check for ronn (ronn -r --pipe README.md > ./bld/README.1)
    elseif (EXISTS /usr/bin/ronn)
        message(STATUS "Generating man page with ronn")
        add_custom_target(man ALL /usr/bin/ronn -r --pipe "${CMAKE_CURRENT_SOURCE_DIR}/README.md" > "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1" DESTINATION man/man1)

    # Check for pandoc (pandoc README.md -t man -o README.1)
    elseif (EXISTS /usr/bin/pandoc)
        message(STATUS "Generating man page with pandoc")
        add_custom_target(man ALL /usr/bin/pandoc "${CMAKE_CURRENT_SOURCE_DIR}/prv/install/README.md" -f markdown -t man -o "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1"
                        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libmembus.1" DESTINATION man/man1)

    endif()
endif()


#====================================================================
# pkg-config
if (SKBUILD)
    message(STATUS "SKBUILD: No pkg-config")
else()
    if(UNIX AND NOT APPLE)
        message(STATUS "Creating pkg-config file")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/prv/install/pkgconfig.pc.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"	@ONLY)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig" COMPONENT admin)
    endif()
endif()


#====================================================================
# CMake package
if (SKBUILD)
    message(STATUS "SKBUILD: No cmake package")
else()
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${APPNAME}-config-version.cmake"
    VERSION ${APPVER}
    COMPATIBILITY AnyNewerVersion
    )

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/prv/install/package-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${APPNAME}-config.cmake" @ONLY)

    set(ConfigPackageLocation lib/cmake/${APPNAME})
    install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${APPNAME}-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${APPNAME}-config-version.cmake"
    DESTINATION
        ${ConfigPackageLocation}
    COMPONENT
        Devel
    )
endif()


#====================================================================
# Install
if (UNIX)
    set(INSTALL_SHAREDIR ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME})

    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/run.sh" RENAME ${PROJECT_NAME} DESTINATION bin COMPONENT admin)
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/prv/sh/" DESTINATION ${INSTALL_SHAREDIR} COMPONENT admin)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/prv/install/info.in ${CMAKE_CURRENT_BINARY_DIR}/info.txt @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/info.txt" DESTINATION ${INSTALL_SHAREDIR} COMPONENT admin)

    # Run ldconfig
    if (NOT APPLE)
        install(CODE "execute_process(COMMAND ldconfig)")
    endif()

    #Manifest file
    set(MANIFESTFILE "MANIFEST.txt")
    if(SKBUILD)
        set(MANIFESTFILE "PYTHON.txt")
    endif()

    #--------------------------------------
    #   !!! MANIFEST MUST BE LAST !!!
    #--------------------------------------
    # Create install manifest
    install(CODE "string(REPLACE \";\" \"\\n\" MY_CMAKE_INSTALL_MANIFEST_CONTENT \"\$\{CMAKE_INSTALL_MANIFEST_FILES\}\")\n\
                file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${MANIFESTFILE} \"\$\{MY_CMAKE_INSTALL_MANIFEST_CONTENT\}\\n${INSTALL_SHAREDIR}/${MANIFESTFILE}\")")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${MANIFESTFILE}" DESTINATION ${INSTALL_SHAREDIR} COMPONENT admin)

    # Use this instead of above if you prefer to manually specify the files/dirs to be removed on uninstall
    # install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/prv/install/${MANIFESTFILE}" DESTINATION INSTALL_SHAREDIR COMPONENT admin)

endif()

#====================================================================
# CPack
# $ cpack --config bld/CPackSourceConfig.cmake
# $ cpack --config bld/CPackConfig.cmake
#

# set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "${APPNAME}@example.com")
set(CPACK_PACKAGE_NAME "${APPNAME}")
set(CPACK_PACKAGE_VERSION "${APPVER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APPDESC}")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/prv/install/deb/postinst")

include(CPack)

