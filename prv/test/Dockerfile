#====================================================================
#
# Build
# $ docker build -t libblank-test -f ./prv/test/Dockerfile .
#
# Run
# $ docker run -it libblank-test
#
#====================================================================

FROM ubuntu:20.04 AS libblank
# FROM debian:10 AS libblank

# Set timezone
ARG TZ="Europe/Berlin"
ENV TZ $TZ
RUN echo ${TZ} > /etc/timezone

# Automated
ENV DEBIAN_FRONTEND=noninteractive

#--------------------------------------------------------------------
# Install Build Dependencies
RUN \
       apt-get -yq update \
    && apt-get -yq install wget build-essential nano git cmake doxygen graphviz ronn

# Install a web browser to view help file (libblank help)
# RUN apt-get -yq install lynx


#--------------------------------------------------------------------
# Copy setup script
COPY . /code/libblank
WORKDIR /code

ENV DOCKERSCRIPT /code/libblank/prv/test/docker.sh

#--------------------------------------------------------------------
# Ensure we have a minimum cmake version
RUN ${DOCKERSCRIPT} check-cmake "3.15" "3.20"

#--------------------------------------------------------------------
# CMake build
RUN ${DOCKERSCRIPT} test-cmake

#--------------------------------------------------------------------
# Conan build
RUN ${DOCKERSCRIPT} test-conan

#--------------------------------------------------------------------
# Gradle build
RUN ${DOCKERSCRIPT} test-gradle

#--------------------------------------------------------------------
# pip build
RUN ${DOCKERSCRIPT} test-pip

#--------------------------------------------------------------------
# npm build
RUN ${DOCKERSCRIPT} test-npm


CMD /bin/bash
