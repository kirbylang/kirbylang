#!/usr/bin/env bash

set -euo pipefail

shopt -s globstar

BIN="${BIN:-./build/kirby-test}"
DIFF="diff -u"

UPDATE=0
VERBOSE=0
FILTER=""

for arg in "$@"; do
    case "$arg" in
        --update)
            UPDATE=1
            ;;
        --verbose|-v)
            VERBOSE=1
            ;;
        *)
            FILTER="$arg"
            ;;
    esac
done

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

SUITES=0
PASS=0
FAIL=0
SKIP=0
UNEXPECTED_SEGFAULTS=0
TOTAL=0

should_run() {
    local name="$1"

    [[ -z "$FILTER" ]] && return 0
    [[ "$name" == *"$FILTER"* ]]
}

run_test() {
    local label="$1"
    local expected="$2"
    local actual="$3"

    TOTAL=$((TOTAL + 1))

    if [[ $UPDATE -eq 1 ]]; then
        cp "$actual" "$expected"
        echo "  🔄 UPDATED: $label"
        PASS=$((PASS + 1))
        return
    fi

    if [[ ! -f "$expected" ]]; then
        echo "  🟨 SKIP $label (missing $(basename "$expected"))"
        SKIP=$((SKIP + 1))
        return
    fi

    if ! $DIFF "$expected" "$actual"; then
        echo "  ❌ $label"
        FAIL=$((FAIL + 1))
    else
        [[ $VERBOSE -eq 1 ]] && echo "  ✅ $label"
        PASS=$((PASS + 1))
    fi
}

run_exit_test() {
    local label="$1"
    local expected="$2"
    local actual_code="$3"

    TOTAL=$((TOTAL + 1))

    if [[ $UPDATE -eq 1 ]]; then
        echo "$actual_code" > "$expected"
        echo "  🔄 UPDATED: $label"
        PASS=$((PASS + 1))
        return
    fi

    if [[ ! -f "$expected" ]]; then
        echo "  🟨 SKIP $label (missing $(basename "$expected"))"
        SKIP=$((SKIP + 1))
        return
    fi

    expected_code=$(cat "$expected")

    if [[ "$actual_code" != "$expected_code" ]]; then
        if [[ "$actual_code" == "139" ]]; then
            message="SEGFAULT"
        elif [[ "$actual_code" == "134" ]]; then
            message="OUT OF BOUNDS"
        else
            message="got $actual_code, expected $expected_code"
        fi

        echo "  ❌ $label ($message)"
        FAIL=$((FAIL + 1))
    else
        [[ $VERBOSE -eq 1 ]] && echo "  ✅ $label"
        PASS=$((PASS + 1))
    fi
}

for file_in in ./tests/**/*.krb; do
    rel_path="${file_in#./tests/}"

    base="${rel_path%.krb}"

    should_run "$base" || continue

    SUITES=$((SUITES + 1))

    expected_out="./tests/$base.krb.out"
    expected_err="./tests/$base.krb.err"
    expected_exit="./tests/$base.krb.exit"
    argv_file="./tests/$base.krb.argv"
    env_file="./tests/$base.krb.env"
    input_file="./tests/$base.krb.in"

    actual_out="$TMP_DIR/$base.krb.out"
    mkdir -p "$(dirname "$actual_out")"

    actual_err="$TMP_DIR/$base.krb.err"
    mkdir -p "$(dirname "$actual_err")"

    extra_args=()
    [[ -f "$argv_file" ]] && read -r -a extra_args < "$argv_file"

    echo "🔬 $file_in ${extra_args[*]:-}"

    set +e

    run_cmd=( "$BIN" -f "$file_in" -- "${extra_args[@]:-}" )

    stdin_cmd=()
    [[ -f "$input_file" ]] && stdin_cmd=( cat "$input_file" )

    if [[ -f "$env_file" ]]; then
        run_cmd=( bash -c '
            set -a
            source "$1"
            set +a
            shift
            exec "$@"
        ' _ "$env_file" "${run_cmd[@]}" )
    fi

    if [[ ${#stdin_cmd[@]} -gt 0 ]]; then
        "${stdin_cmd[@]}" | "${run_cmd[@]}" >"$actual_out" 2>"$actual_err"
    else
        "${run_cmd[@]}" >"$actual_out" 2>"$actual_err"
    fi

    exit_code=$?
    set -e

    run_test      "stdout" "$expected_out" "$actual_out"
    run_test      "stderr" "$expected_err" "$actual_err"
    run_exit_test "exit code" "$expected_exit" "$exit_code"

    if [[ $exit_code -eq 139 ]]; then
        UNEXPECTED_SEGFAULTS=$((UNEXPECTED_SEGFAULTS + 1))
        printf "\e[1;31mTEST SEGFAULTED\e[0m\n"
    fi

    [[ $VERBOSE -eq 1 ]] && echo ""
done

echo
echo "Total: $TOTAL | Passed: $PASS | Failed: $FAIL | Skipped: $SKIP | Suites: $SUITES"

if [[ $UNEXPECTED_SEGFAULTS -ne 0 ]]; then
    echo
    printf "\e[1;31mFAILURE: ONE OR MORE TESTS SEGFAULTED!\e[0m\n"
    echo
fi

if [[ $FAIL -ne 0 || $UNEXPECTED_SEGFAULTS -ne 0 ]]; then
    exit 1
fi
