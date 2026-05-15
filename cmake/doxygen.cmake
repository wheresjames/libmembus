
#====================================================================
# doxygen.cmake — Defines the "doc" target that runs Doxygen.
#
# Called from the root CMakeLists.txt when LIBMEMBUS_BUILD_DOCS=ON.
# Output lands in ${CMAKE_BINARY_DIR}/docs/html/index.html.
#====================================================================

find_package(Doxygen OPTIONAL_COMPONENTS dot)

if (NOT DOXYGEN_FOUND)
    message(STATUS " [!] Doxygen not found — skipping 'doc' target")
    return()
endif()

message(STATUS " [-] Doxygen ${DOXYGEN_VERSION} found at ${DOXYGEN_EXECUTABLE}")


# ── Project metadata ────────────────────────────────────────────────────────

set(DOXYGEN_PROJECT_NAME   "${PROJECT_NAME}")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_PROJECT_BRIEF  "${PROJECT_DESCRIPTION}")


# ── Input ───────────────────────────────────────────────────────────────────

# Show paths relative to the include/ root in the generated docs
set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_SOURCE_DIR}/include")

set(DOXYGEN_FILE_PATTERNS    "*.h")
set(DOXYGEN_RECURSIVE        YES)

# Exclude the internal header — it is not part of the public API
set(DOXYGEN_EXCLUDE_PATTERNS "*-internal*")

# docs/mainpage.md is the Doxygen front page (README.md uses GitHub-style
# Markdown anchors that Doxygen cannot resolve as cross-references)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/docs/mainpage.md")


# ── Preprocessing ───────────────────────────────────────────────────────────

set(DOXYGEN_ENABLE_PREPROCESSING YES)
set(DOXYGEN_MACRO_EXPANSION      YES)

# Resolve the LIBMEMBUS_NS macro so docs show the public namespace name "mmb"
set(DOXYGEN_PREDEFINED        "LIBMEMBUS_NS=mmb")
set(DOXYGEN_EXPAND_AS_DEFINED "LIBMEMBUS_NS")

set(DOXYGEN_MARKDOWN_SUPPORT    YES)
set(DOXYGEN_JAVADOC_AUTOBRIEF   YES)   # First sentence of /** */ is the brief
set(DOXYGEN_BUILTIN_STL_SUPPORT YES)


# ── Extraction ──────────────────────────────────────────────────────────────

set(DOXYGEN_EXTRACT_ALL     NO)    # Only entities that carry Doxygen comments
set(DOXYGEN_EXTRACT_STATIC  YES)
set(DOXYGEN_EXTRACT_PRIVATE NO)
set(DOXYGEN_SHOW_USED_FILES NO)    # Keeps output concise


# ── Output ──────────────────────────────────────────────────────────────────

set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs")
set(DOXYGEN_GENERATE_HTML    YES)
set(DOXYGEN_HTML_OUTPUT      "html")
set(DOXYGEN_GENERATE_LATEX   NO)


# ── HTML appearance ─────────────────────────────────────────────────────────

set(DOXYGEN_GENERATE_TREEVIEW  YES)    # Side-bar navigation panel
set(DOXYGEN_HTML_COLORSTYLE    AUTO_LIGHT)  # Respects system light/dark preference
set(DOXYGEN_SEARCHENGINE       YES)
set(DOXYGEN_SORT_MEMBER_DOCS   NO)    # Preserve declaration order in headers
set(DOXYGEN_ALPHABETICAL_INDEX YES)


# ── Graphs (Graphviz dot) ───────────────────────────────────────────────────
# find_package(Doxygen OPTIONAL_COMPONENTS dot) sets DOXYGEN_DOT_FOUND in some
# CMake versions and Doxygen_dot_FOUND in others.  Fall back to find_program so
# detection is reliable across all versions.

if (NOT DOXYGEN_DOT_FOUND AND NOT Doxygen_dot_FOUND)
    find_program(DOXYGEN_DOT_EXECUTABLE dot)
    if (DOXYGEN_DOT_EXECUTABLE)
        set(DOXYGEN_DOT_FOUND TRUE)
    endif()
endif()

if (DOXYGEN_DOT_FOUND OR Doxygen_dot_FOUND)
    message(STATUS " [-] Doxygen: Graphviz dot found — class/include graphs enabled")
    get_filename_component(_dot_dir "${DOXYGEN_DOT_EXECUTABLE}" DIRECTORY)
    set(DOXYGEN_HAVE_DOT            YES)
    set(DOXYGEN_DOT_PATH            "${_dot_dir}")
    set(DOXYGEN_DOT_IMAGE_FORMAT    "png")
    set(DOXYGEN_DOT_TRANSPARENT     YES)
    set(DOXYGEN_DOT_GRAPH_MAX_NODES 30)
    set(DOXYGEN_COLLABORATION_GRAPH YES)
    set(DOXYGEN_INCLUDE_GRAPH       YES)
    set(DOXYGEN_INCLUDED_BY_GRAPH   YES)
    set(DOXYGEN_CALL_GRAPH          NO)
    set(DOXYGEN_CALLER_GRAPH        NO)
    # The graph_legend.dot triggers a pathplan crash in Graphviz 2.42; disable it.
    set(DOXYGEN_GENERATE_LEGEND     NO)
else()
    message(STATUS " [-] Doxygen: Graphviz dot not found — graphs disabled")
    set(DOXYGEN_HAVE_DOT NO)
endif()


# ── Warnings ────────────────────────────────────────────────────────────────

set(DOXYGEN_QUIET                YES)
set(DOXYGEN_WARNINGS             YES)
set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
# WARN_NO_PARAMDOC is intentionally NO: simple one-liner getters describe their
# return value in the brief text rather than with an explicit @returns tag.
set(DOXYGEN_WARN_NO_PARAMDOC     NO)
set(DOXYGEN_WARN_AS_ERROR        NO)


# ── Target ──────────────────────────────────────────────────────────────────

doxygen_add_docs(doc
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/docs/mainpage.md"
    COMMENT "Generating API docs — open ${CMAKE_BINARY_DIR}/docs/html/index.html"
)

message(STATUS " [-] Doxygen 'doc' target added (cmake --build ./build --target doc)")
