#!/usr/bin/env bash

set -euo pipefail

BIN="${BIN:-./build/kirby-test}"
RUNTIME_BIN="${RUNTIME_BIN:-./build/kirby-test-runtime}"
DIFF="diff -u"

UPDATE=0
VERBOSE=0
PACKAGED=0
FILTER=""

for arg in "$@"; do
    case "$arg" in
        --update)
            UPDATE=1
            ;;
        --verbose|-v)
            VERBOSE=1
            ;;
        --packaged)
            PACKAGED=1
            ;;
        *)
            FILTER="$arg"
            ;;
    esac
done

if [[ $PACKAGED -eq 1 && $UPDATE -eq 1 ]]; then
    echo "--update can't be used with --packaged: snapshots come from 'krb run'" >&2
    exit 1
fi

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

# For a program that fails to build. 'krb run' prints the standard library's
# bytecode before it reports a compile error, and 'krb build' never loads
# anything, so the build's stderr must be the end of the snapshot.
run_suffix_test() {
    local label="$1"
    local expected="$2"
    local actual="$3"

    TOTAL=$((TOTAL + 1))

    if [[ ! -f "$expected" ]]; then
        echo "  🟨 SKIP $label (missing $(basename "$expected"))"
        SKIP=$((SKIP + 1))
        return
    fi

    # The trailing "x" keeps the final newlines, which $(...) would remove.
    local expected_text actual_text
    expected_text=$(cat "$expected"; printf x)
    actual_text=$(cat "$actual"; printf x)

    if [[ "$expected_text" == *"$actual_text" ]]; then
        [[ $VERBOSE -eq 1 ]] && echo "  ✅ $label"
        PASS=$((PASS + 1))
    else
        $DIFF "$expected" "$actual" || true
        echo "  ❌ $label (build output is not the end of the snapshot)"
        FAIL=$((FAIL + 1))
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

# Collect first — don't pipe into the loop. krb reads stdin (prompt(), the
# `run` tests), and a while-read loop would let it swallow the file list.
files=()
while IFS= read -r file; do
    files+=("$file")
done < <(find ./tests -type f -name '*.krb' | LC_ALL=C sort)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "No tests found" >&2
    exit 1
fi

for file_in in "${files[@]}"; do
    rel_path="${file_in#./}"
    rel_path="${rel_path#tests/}"

    base="${rel_path%.krb}"

    should_run "$base" || continue

    SUITES=$((SUITES + 1))

    expected_out="./tests/$base.krb.out"
    expected_err="./tests/$base.krb.err"
    expected_exit="./tests/$base.krb.exit"
    argv_file="./tests/$base.krb.argv"
    env_file="./tests/$base.krb.env"
    input_file="./tests/$base.krb.in"
    skip_packaged_file="./tests/$base.krb.skip-packaged"

    actual_out="$TMP_DIR/$base.krb.out"
    mkdir -p "$(dirname "$actual_out")"

    actual_err="$TMP_DIR/$base.krb.err"
    mkdir -p "$(dirname "$actual_err")"

    # One argument per line, so an argument can hold spaces or be empty.
    extra_args=()
    if [[ -f "$argv_file" ]]; then
        while IFS= read -r line || [[ -n "$line" ]]; do
            extra_args+=("$line")
        done < "$argv_file"
    fi

    echo "🔬 $file_in ${extra_args[*]:-}"

    # The program's own argv differs when it isn't started by 'krb run'.
    if [[ $PACKAGED -eq 1 && -f "$skip_packaged_file" ]]; then
        echo "  🟨 SKIP packaged ($(basename "$skip_packaged_file"))"
        TOTAL=$((TOTAL + 3))
        SKIP=$((SKIP + 3))
        continue
    fi

    set +e

    run_cmd=( "$BIN" run "$file_in" ${extra_args[@]+"${extra_args[@]}"} )

    # --packaged builds the program into an executable and runs that instead.
    build_failed=0
    if [[ $PACKAGED -eq 1 ]]; then
        app="$TMP_DIR/$base.app"
        "$BIN" build "$file_in" -o "$app" --runtime "$RUNTIME_BIN" \
            >"$actual_out" 2>"$actual_err" </dev/null
        build_exit=$?

        if [[ $build_exit -eq 0 ]]; then
            run_cmd=( "$app" ${extra_args[@]+"${extra_args[@]}"} )
        else
            build_failed=1
        fi
    fi

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

    if [[ $build_failed -eq 1 ]]; then
        exit_code=$build_exit
    else
        if [[ ${#stdin_cmd[@]} -gt 0 ]]; then
            "${stdin_cmd[@]}" | "${run_cmd[@]}" >"$actual_out" 2>"$actual_err"
        else
            "${run_cmd[@]}" >"$actual_out" 2>"$actual_err"
        fi

        exit_code=$?
    fi

    set -e

    run_test      "stdout" "$expected_out" "$actual_out"

    if [[ $build_failed -eq 1 ]]; then
        run_suffix_test "stderr" "$expected_err" "$actual_err"
    else
        run_test "stderr" "$expected_err" "$actual_err"
    fi

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
