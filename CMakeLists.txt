cmake_minimum_required(VERSION 3.22)

project(kirby LANGUAGES C)

enable_testing()

option(ENABLE_MEMCHECK "Enable AddressSanitizer memory checking" OFF)

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

set(KIRBY_SOURCES
    src/asserts.c
    src/ast.c
    src/chunk.c
    src/common.c
    src/compiler.c
    src/debug.c
    src/hashtable.c
    src/lexer.c
    src/compiled_unit.c
    src/gc.c
    src/loader.c
    src/native.c
    src/object.c
    src/parser.c
    src/scanner.c
    src/strbuf.c
    src/stringset.c
    src/token_stream.c
    src/token.c
    src/value.c
    src/vm.c
    ${VERSION_C}
)

# -----------------------------------------------------------------------------
# Shared configuration helpers
# -----------------------------------------------------------------------------

function(configure_kirby_target TARGET_NAME)

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

add_library(kirby_core STATIC
    ${KIRBY_SOURCES}
)

target_compile_definitions(kirby_core
    PRIVATE
        # DEBUG_TRACE_EXECUTION
)


configure_kirby_target(kirby_core)
enable_coverage(kirby_core)

# -----------------------------------------------------------------------------
# Shared runtime library
# -----------------------------------------------------------------------------

add_library(kirbylib SHARED
    ${KIRBY_SOURCES}
)

set_target_properties(kirbylib PROPERTIES
    OUTPUT_NAME kirby
)

configure_kirby_target(kirbylib)
enable_coverage(kirbylib)

# -----------------------------------------------------------------------------
# Debug runtime library
# -----------------------------------------------------------------------------

add_library(kirby_core_debug STATIC
    ${KIRBY_SOURCES}
)

target_compile_definitions(kirby_core_debug
    PRIVATE
        DEBUG_PRINT_CODE
        DEBUG_STRESS_GC
        # DEBUG_TRACE_EXECUTION
)

configure_kirby_target(kirby_core_debug)
enable_coverage(kirby_core_debug)

if(ENABLE_MEMCHECK)
    target_compile_options(kirby_core_debug PUBLIC -fsanitize=address)
    target_link_options(kirby_core_debug PUBLIC -fsanitize=address)
endif()

# -----------------------------------------------------------------------------
# Main executable
# -----------------------------------------------------------------------------

add_executable(krb
    src/main.c
)

target_link_libraries(krb
    PRIVATE
        kirby_core
)

configure_kirby_target(krb)
enable_coverage(krb)

# Optional debug flags
# target_compile_definitions(kirby PRIVATE DEBUG_TRACE_EXECUTION)
# target_compile_definitions(kirby PRIVATE DEBUG_STRESS_GC)
# target_compile_definitions(kirby PRIVATE DEBUG_LOG_GC)

# -----------------------------------------------------------------------------
# Debug executable
# -----------------------------------------------------------------------------

add_executable(kirby-test
    src/main.c
)

target_link_libraries(kirby-test
    PRIVATE
        kirby_core_debug
)

configure_kirby_target(kirby-test)
enable_coverage(kirby-test)

if(ENABLE_MEMCHECK)
    target_compile_options(kirby-test PUBLIC -fsanitize=address)
    target_link_options(kirby-test PUBLIC -fsanitize=address)
endif()

# -----------------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------------

if(BUILD_TESTS)

    add_executable(kirby-version-unit-test
        unit/version.c
    )

    target_link_libraries(kirby-version-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-version-unit-test)
    enable_coverage(kirby-version-unit-test)

    add_test(
        NAME kirby-version-unit-test
        COMMAND kirby-version-unit-test
    )

    add_executable(kirby-token-unit-test
        unit/token.c
    )

    target_link_libraries(kirby-token-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-token-unit-test)
    enable_coverage(kirby-token-unit-test)

    add_test(
        NAME kirby-token-unit-test
        COMMAND kirby-token-unit-test
    )

    add_executable(kirby-stringset-unit-test
        unit/stringset.c
    )

    target_link_libraries(kirby-stringset-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-stringset-unit-test)
    enable_coverage(kirby-stringset-unit-test)

    add_test(
        NAME kirby-stringset-unit-test
        COMMAND kirby-stringset-unit-test
    )

    add_executable(kirby-value-unit-test
        unit/value.c
    )

    target_link_libraries(kirby-value-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-value-unit-test)
    enable_coverage(kirby-value-unit-test)

    add_test(
        NAME kirby-value-unit-test
        COMMAND kirby-value-unit-test
    )

    add_executable(kirby-parser-unit-test
        unit/parser.c
    )

    target_link_libraries(kirby-parser-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-parser-unit-test)
    enable_coverage(kirby-parser-unit-test)

    add_test(
        NAME kirby-parser-unit-test
        COMMAND kirby-parser-unit-test
    )

    add_executable(kirby-lexer-unit-test
        unit/lexer.c
    )

    target_link_libraries(kirby-lexer-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-lexer-unit-test)
    enable_coverage(kirby-lexer-unit-test)

    add_test(
        NAME kirby-lexer-unit-test
        COMMAND kirby-lexer-unit-test
    )

    add_executable(kirby-vm-field-limit-unit-test
        unit/vm_field_limit.c
    )

    target_link_libraries(kirby-vm-field-limit-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-vm-field-limit-unit-test)
    enable_coverage(kirby-vm-field-limit-unit-test)

    add_test(
        NAME kirby-vm-field-limit-unit-test
        COMMAND kirby-vm-field-limit-unit-test
    )

    add_executable(kirby-vm-invoke-unit-test
        unit/vm_invoke.c
    )

    target_link_libraries(kirby-vm-invoke-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-vm-invoke-unit-test)
    enable_coverage(kirby-vm-invoke-unit-test)

    add_test(
        NAME kirby-vm-invoke-unit-test
        COMMAND kirby-vm-invoke-unit-test
    )

    add_executable(kirby-vm-repl-immutability-unit-test
        unit/vm_repl_immutability.c
    )

    target_link_libraries(kirby-vm-repl-immutability-unit-test
        PRIVATE
            kirby_core_debug
    )

    configure_kirby_target(kirby-vm-repl-immutability-unit-test)
    enable_coverage(kirby-vm-repl-immutability-unit-test)

    add_test(
        NAME kirby-vm-repl-immutability-unit-test
        COMMAND kirby-vm-repl-immutability-unit-test
    )
endif()
