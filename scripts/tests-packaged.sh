#!/usr/bin/env bash

# Runs a small set of the tests again as built programs. Each one is built into
# an executable with `krb build` and run, then compared with the same snapshots
# the test already has. tests.sh does the comparing, with run-packaged.sh
# standing in for BIN.
#
# The snapshots hold the bytecode listing, so the runtime must be a build that
# prints it, such as kirby-test-runtime.
#
# A test that prints @argv(0) can't be in the list, because a built program is
# started from a different path. A test that ends in a compile error can't be
# either, because `krb build` never loads the standard library and so doesn't
# print its bytecode before the error. test-build.sh covers that.
#
#   ./scripts/tests-packaged.sh
#   ./scripts/tests-packaged.sh --verbose

set -euo pipefail

export REAL_BIN="${BIN:-./build/kirby-test}"
export RUNTIME_BIN="${RUNTIME_BIN:-./build/kirby-test-runtime}"

here="$(cd "$(dirname "$0")" && pwd)"

TESTS=(
    # Every opcode that appears in any test
    run/block_expressions/block_expression_locals_in_operand_positions.krb
    run/types/function_returns_on_all_paths.krb
    run/operators/divide_by_zero_left.krb
    run/operators/equals_lte_false.krb
    run/closures/upvalue_linked.krb
    run/locals_bookkeeping.krb
    run/operators/modulo.krb
    run/operators/nullish.krb

    # The largest programs: the most code and the most constants
    run/native_functions/native_fns.krb
    run/strings/string_concat_chain_long.krb
    run/types/impl_target_name_survives_arena_growth.krb

    # What the runtime does around the program: arguments, an argument with
    # spaces or empty, stdin, environment variables, an exit code, and a runtime
    # error that the VM reports back to the runtime
    run/native_functions/native_fn_script_args.krb
    run/native_functions/native_fn_script_args_with_spaces_and_empty.krb
    run/native_functions/native_fn_prompt_call.krb
    run/native_functions/native_fn_getenv_call.krb
    run/native_functions/native_fn_exit_call.krb
    run/arrays/array_index_get_out_of_bounds.krb

    # Public and private members
    run/structs/struct_private_field_self.krb
    run/structs/struct_pub_static_method.krb
)

BIN="$here/run-packaged.sh" "$here/tests.sh" "${TESTS[@]}" "$@"
