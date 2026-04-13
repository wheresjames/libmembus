
# ---- Example consumer ----
# add_executable(app src/main.cpp)
# target_link_libraries(app PRIVATE temporal::capi)

#====================================================================

include(FetchContent)

FetchContent_Declare(
        temporal
        GIT_REPOSITORY      https://github.com/boa-dev/temporal.git
        GIT_TAG             ${LIB3_TEMPORAL_VERSION}
        GIT_PROGRESS        TRUE
    )
FetchContent_MakeAvailable(temporal)

# ---- Find cargo ----
find_program(CARGO_EXECUTABLE cargo REQUIRED)

# ---- Cargo profile selection (Debug/Release) ----
if(CMAKE_BUILD_TYPE)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _temporal_cfg)
else()
    set(_temporal_cfg "release")
endif()

if(_temporal_cfg STREQUAL "debug")
    set(_cargo_profile "debug")
    set(_cargo_release_flag "")
else()
    set(_cargo_profile "release")
    set(_cargo_release_flag "--release")
endif()

# ---- Paths ----
get_filename_component(TEMPORAL_WORKSPACE_DIR "${temporal_SOURCE_DIR}" ABSOLUTE)
set(TEMPORAL_CAPI_DIR      "${TEMPORAL_WORKSPACE_DIR}/temporal_capi")

# Temporal moved its generated headers from temporal_capi/include to
# temporal_capi/bindings/c (v0.2.0). Probe for the folder instead of
# hardcoding the old location so pinning newer tags keeps working.
set(_temporal_candidate_include_dirs
    "${TEMPORAL_CAPI_DIR}/include"
    "${TEMPORAL_CAPI_DIR}/bindings/c"
)
set(TEMPORAL_CAPI_INCLUDE_DIR "")
foreach(_dir IN LISTS _temporal_candidate_include_dirs)
    if(EXISTS "${_dir}")
        set(TEMPORAL_CAPI_INCLUDE_DIR "${_dir}")
        break()
    endif()
endforeach()

if(NOT TEMPORAL_CAPI_INCLUDE_DIR)
    message(FATAL_ERROR "Could not locate temporal_capi headers under ${TEMPORAL_CAPI_DIR}")
endif()

# Static library filename per-platform
if(WIN32)
    set(_temporal_lib_name "temporal_capi.lib")
else()
    set(_temporal_lib_name "libtemporal_capi.a")
endif()

# Generate a shim crate that links against the upstream temporal_capi, wires in
# a libc-backed allocator and panic handler, and emits the static library we
# consume from C++.
set(TEMPORAL_CAPI_SHIM_DIR "${CMAKE_BINARY_DIR}/temporal_capi_static")
set(_temporal_shim_src_dir "${TEMPORAL_CAPI_SHIM_DIR}/src")
file(MAKE_DIRECTORY "${_temporal_shim_src_dir}")

set(_temporal_shim_cargo "${TEMPORAL_CAPI_SHIM_DIR}/Cargo.toml")
set(_temporal_shim_lib   "${_temporal_shim_src_dir}/lib.rs")

set(_temporal_shim_cargo_contents [=[
[package]
name = "temporal_capi_static"
version = "0.1.0"
edition = "2021"

[lib]
name = "temporal_capi"
crate-type = ["staticlib"]

[dependencies]
temporal_capi = { path = "@TEMPORAL_CAPI_DIR@", features = ["compiled_data", "zoneinfo64"] }

[profile.dev]
panic = "abort"

[profile.release]
panic = "abort"
]=])
string(CONFIGURE "${_temporal_shim_cargo_contents}"
       _temporal_shim_cargo_contents @ONLY)
file(WRITE "${_temporal_shim_cargo}" "${_temporal_shim_cargo_contents}")

set(_temporal_shim_lib_contents [=[
pub use temporal_capi::*;
]=])
file(WRITE "${_temporal_shim_lib}" "${_temporal_shim_lib_contents}")

# Cargo output location:
#   <shim>/target/<profile>/<lib>
set(_temporal_lib_path
    "${TEMPORAL_CAPI_SHIM_DIR}/target/${_cargo_profile}/${_temporal_lib_name}"
)

# ---- Build temporal_capi via cargo ----
add_custom_command(
    OUTPUT "${_temporal_lib_path}"
    COMMAND "${CARGO_EXECUTABLE}" build ${_cargo_release_flag}
            --manifest-path "${_temporal_shim_cargo}"
    WORKING_DIRECTORY "${TEMPORAL_CAPI_SHIM_DIR}"
    COMMENT "Building temporal_capi with Cargo"
    VERBATIM
)

add_custom_target(temporal_capi_cargo ALL
    DEPENDS "${_temporal_lib_path}"
)

# ---- Expose a CMake target: temporal::capi ----
add_library(temporal_capi STATIC IMPORTED GLOBAL)
add_library(temporal::capi ALIAS temporal_capi)

add_dependencies(temporal_capi temporal_capi_cargo)

# Multi-config-friendly imported location
set_target_properties(temporal_capi PROPERTIES
    IMPORTED_LOCATION "${_temporal_lib_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${TEMPORAL_CAPI_INCLUDE_DIR}"
)

# Typical extra libs needed on Unix when linking Rust staticlibs
if(UNIX AND NOT APPLE)
    target_link_libraries(temporal_capi INTERFACE dl pthread)
endif()

