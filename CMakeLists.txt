cmake_minimum_required(VERSION 3.22)

project(clox LANGUAGES C)

enable_testing()

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

# -----------------------------------------------------------------------------
# Compiler configuration
# -----------------------------------------------------------------------------

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# -----------------------------------------------------------------------------
# Version generation
# -----------------------------------------------------------------------------

set(VERSION_TXT ${CMAKE_SOURCE_DIR}/VERSION.txt)

# Generate into build tree
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
set(VERSION_C ${GENERATED_DIR}/version.c)

add_custom_command(
    OUTPUT ${VERSION_C}
    COMMAND bash
        ${CMAKE_SOURCE_DIR}/scripts/generate_version_c.sh
        ${VERSION_C}
        ${VERSION_TXT}
    DEPENDS
        ${VERSION_TXT}
        ${CMAKE_SOURCE_DIR}/scripts/generate_version_c.sh
    COMMENT "Generating version.c"
)

# -----------------------------------------------------------------------------
# Shared source list
# -----------------------------------------------------------------------------

set(CLOX_SOURCES
    src/asserts.c
    src/ast.c
    src/chunk.c
    src/compiler.c
    src/debug.c
    src/hashtable.c
    src/lexer.c
    src/memory.c
    src/native.c
    src/object.c
    src/parser.c
    src/scanner.c
    src/token_stream.c
    src/token.c
    src/value.c
    src/vm.c
    ${VERSION_C}
)

# -----------------------------------------------------------------------------
# Shared configuration helpers
# -----------------------------------------------------------------------------

function(configure_clox_target TARGET_NAME)

    target_include_directories(${TARGET_NAME}
        PUBLIC
            ${CMAKE_SOURCE_DIR}/src
            ${GENERATED_DIR}
    )

    target_compile_options(${TARGET_NAME}
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_link_libraries(${TARGET_NAME}
        PUBLIC
            readline
            m
    )

endfunction()

function(enable_coverage TARGET_NAME)

    if(ENABLE_COVERAGE)

        target_compile_options(${TARGET_NAME}
            PRIVATE
                --coverage
                -O0
                -g
        )

        target_link_options(${TARGET_NAME}
            PRIVATE
                --coverage
        )

    endif()

endfunction()

# -----------------------------------------------------------------------------
# Main runtime library (static)
# -----------------------------------------------------------------------------

add_library(clox_core STATIC
    ${CLOX_SOURCES}
)

# target_compile_definitions(clox_core
#     PRIVATE
#         DEBUG_TRACE_EXECUTION
# )


configure_clox_target(clox_core)
enable_coverage(clox_core)

# -----------------------------------------------------------------------------
# Shared runtime library
# -----------------------------------------------------------------------------

add_library(cloxlib SHARED
    ${CLOX_SOURCES}
)

set_target_properties(cloxlib PROPERTIES
    OUTPUT_NAME clox
)

configure_clox_target(cloxlib)
enable_coverage(cloxlib)

# -----------------------------------------------------------------------------
# Debug runtime library
# -----------------------------------------------------------------------------

add_library(clox_core_debug STATIC
    ${CLOX_SOURCES}
)

target_compile_definitions(clox_core_debug
    PRIVATE
        DEBUG_PRINT_CODE
        DEBUG_STRESS_GC
)

configure_clox_target(clox_core_debug)
enable_coverage(clox_core_debug)

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------

add_executable(clox
    src/main.c
)

target_link_libraries(clox
    PRIVATE
        clox_core
)

configure_clox_target(clox)
enable_coverage(clox)

# Optional debug flags
# target_compile_definitions(clox PRIVATE DEBUG_TRACE_EXECUTION)
# target_compile_definitions(clox PRIVATE DEBUG_STRESS_GC)
# target_compile_definitions(clox PRIVATE DEBUG_LOG_GC)

# -----------------------------------------------------------------------------
# Debug executable
# -----------------------------------------------------------------------------

add_executable(clox-test
    src/main.c
)

target_link_libraries(clox-test
    PRIVATE
        clox_core_debug
)

configure_clox_target(clox-test)
enable_coverage(clox-test)

# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------

if(BUILD_TESTS)

    add_executable(clox-version-unit-test
        unit/version.c
    )

    target_link_libraries(clox-version-unit-test
        PRIVATE
            clox_core_debug
    )

    configure_clox_target(clox-version-unit-test)
    enable_coverage(clox-version-unit-test)

    add_test(
        NAME clox-version-unit-test
        COMMAND clox-version-unit-test
    )

    add_executable(clox-token-unit-test
        unit/token.c
        src/token.c
    )

    target_link_libraries(clox-token-unit-test
        PRIVATE
            clox_core_debug
    )

    configure_clox_target(clox-token-unit-test)
    enable_coverage(clox-token-unit-test)

    add_test(
        NAME clox-token-unit-test
        COMMAND clox-token-unit-test
    )

    add_executable(clox-value-unit-test
        unit/value.c
    )

    target_link_libraries(clox-value-unit-test
        PRIVATE
            clox_core_debug
    )

    configure_clox_target(clox-value-unit-test)
    enable_coverage(clox-value-unit-test)

    add_test(
        NAME clox-value-unit-test
        COMMAND clox-value-unit-test
    )

    add_executable(clox-parser-unit-test
        unit/parser.c
    )

    target_link_libraries(clox-parser-unit-test
        PRIVATE
            clox_core_debug
    )

    configure_clox_target(clox-parser-unit-test)
    enable_coverage(clox-parser-unit-test)

    add_test(
        NAME clox-parser-unit-test
        COMMAND clox-parser-unit-test
    )

endif()
