#!/usr/bin/env bash

set -euo pipefail

BIN="${BIN:-./build/kirby-test}"

# The folder that holds the tests. Any .argv file under it is a test.
TESTS_DIR="${TESTS_DIR:-./tests}"
TESTS_DIR="${TESTS_DIR%/}"
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

# A test is a `<name>.argv` file in tests/. The files that belong to it are
# named after it:
#
#   <name>.argv  the arguments for BIN, one per line. $file is the path of <name>
#   <name>       the program or input the test uses, if any
#   <name>.out   expected stdout
#   <name>.err   expected stderr
#   <name>.exit  expected exit code
#   <name>.in    stdin, which is empty without this file
#   <name>.env   environment variables, sourced before BIN runs
SIDECARS=".out .err .exit .in .env"

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
            message="ABORTED"
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

# Every file in the tests folder must belong to a test. Dotfiles, dot folders
# and the README do not.
check_every_file_is_claimed() {
    local all="$TMP_DIR/all-files.txt"
    local claimed="$TMP_DIR/claimed-files.txt"

    find "$TESTS_DIR" -mindepth 1 -name '.*' -prune -o \
        -type f ! -path "$TESTS_DIR/README.md" -print \
        | LC_ALL=C sort >"$all"

    find "$TESTS_DIR" -mindepth 1 -name '.*' -prune -o \
        -type f -name '*.argv' -print | while IFS= read -r argv_path; do
        stem="${argv_path%.argv}"
        echo "$argv_path"

        if [[ -e "$stem" ]]; then
            echo "$stem"
        fi

        for suffix in $SIDECARS; do
            if [[ -e "$stem$suffix" ]]; then
                echo "$stem$suffix"
            fi
        done
    done | LC_ALL=C sort >"$claimed"

    local unclaimed
    unclaimed=$(LC_ALL=C comm -23 "$all" "$claimed")

    if [[ -n "$unclaimed" ]]; then
        echo "These files are not part of a test. A test is a file ending in .argv:" >&2
        echo "$unclaimed" | sed 's/^/  /' >&2
        exit 1
    fi
}

check_every_file_is_claimed

# Collect first — don't pipe into the loop. A test can read stdin, and a
# while-read loop would let it swallow the list of tests.
argv_files=()
while IFS= read -r argv_path; do
    argv_files+=("$argv_path")
done < <(find "$TESTS_DIR" -mindepth 1 -name '.*' -prune -o \
    -type f -name '*.argv' -print | LC_ALL=C sort)

if [[ ${#argv_files[@]} -eq 0 ]]; then
    echo "No tests found" >&2
    exit 1
fi

for argv_file in "${argv_files[@]}"; do
    name="${argv_file#"$TESTS_DIR"/}"
    name="${name%.argv}"

    should_run "$name" || continue

    SUITES=$((SUITES + 1))

    file="$TESTS_DIR/$name"
    expected_out="$file.out"
    expected_err="$file.err"
    expected_exit="$file.exit"
    env_file="$file.env"
    input_file="$file.in"

    actual_out="$TMP_DIR/$name.out"
    mkdir -p "$(dirname "$actual_out")"

    actual_err="$TMP_DIR/$name.err"
    mkdir -p "$(dirname "$actual_err")"

    echo "🔬 $file"

    # One argument per line, expanded like text inside double quotes: $file and
    # $(...) work and nothing is split, so an argument can hold spaces or be
    # empty. Each line is expanded in a subshell, so a mistake fails this test
    # and not the whole run. Reading from fd 3 keeps a $(...) from consuming
    # the rest of the file.
    args=()
    args_ok=1
    while IFS= read -r line <&3 || [[ -n "$line" ]]; do
        raw_line="$line"
        line=${line//\"/\\\"}

        if ! ( eval "value=\"$line\"" && printf '%s' "$value" ) >"$TMP_DIR/arg"; then
            args_ok=0
            break
        fi

        args+=("$(cat "$TMP_DIR/arg")")
    done 3<"$argv_file"

    if [[ $args_ok -eq 0 ]]; then
        TOTAL=$((TOTAL + 1))
        FAIL=$((FAIL + 1))
        echo "  ❌ $(basename "$argv_file") could not expand: $raw_line"
        continue
    fi

    set +e

    run_cmd=( "$BIN" ${args[@]+"${args[@]}"} )

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
        "${run_cmd[@]}" >"$actual_out" 2>"$actual_err" </dev/null
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
