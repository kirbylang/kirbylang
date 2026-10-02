#!/usr/bin/env bash

# Tests of the arguments a script sees. However a script is started, @argv(0) is
# the program and @argv(1) onward are the script's own arguments:
#
#   krb run file.krb one two      @argc() is 3, @argv(0) is file.krb
#   krb example name one two      @argc() is 3, @argv(0) is <examples>/name.krb
#   krb exec '<code>' one two     @argc() is 3, @argv(0) is exec
#   krb repl                      @argc() is 1, @argv(0) is repl
#   ./built-program one two       @argc() is 3, @argv(0) is ./built-program

set -uo pipefail

BIN="${BIN:-./build/krb}"
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

PASS=0
FAIL=0

check() {
    local name="$1"
    local expected="$2"
    local actual="$3"

    if [[ "$expected" == "$actual" ]]; then
        PASS=$((PASS + 1))
        echo "  ✅ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ❌ $name"
        echo "     expected: ${expected//$'\n'/|}"
        echo "     actual:   ${actual//$'\n'/|}"
    fi
}

# Runs a command and sets $out (stdout) and $code (exit code).
capture() {
    out=$("$@" 2>/dev/null </dev/null)
    code=$?
}

cd "$WORK_DIR"

cat >show.krb <<'EOF'
@println(@argc());
@println(@argv(1));
@println(@argv(2));
@println(@argv(3));
EOF

printf '@println(@argv(0));\n' >zero.krb

# A project, for the commands that read kirby.project.toml.
mkdir -p proj/bin proj/examples
cp show.krb proj/bin/main.krb
cp show.krb proj/examples/show.krb
cp zero.krb proj/examples/zero.krb
printf 'bin = "bin/main.krb"\nexamples = "examples"\n' >proj/kirby.project.toml

SHOW_CODE='@println(@argc()); @println(@argv(1)); @println(@argv(2)); @println(@argv(3));'

TWO_ARGS=$'3\none\ntwo\nnil'
NO_ARGS=$'1\nnil\nnil\nnil'

"$BIN" build show.krb -o show-app 2>/dev/null
"$BIN" build zero.krb -o ./zero-app 2>/dev/null

echo "Script arguments start at @argv(1)"

capture "$BIN" run show.krb one two
check "krb run file one two" "$TWO_ARGS" "$out"

capture bash -c "cd proj && '$BIN' run -- one two"
check "krb run -- one two (the project's bin)" "$TWO_ARGS" "$out"

capture bash -c "cd proj && '$BIN' example show one two"
check "krb example show one two" "$TWO_ARGS" "$out"

capture "$BIN" exec "$SHOW_CODE" one two
check "krb exec '<code>' one two" "$TWO_ARGS" "$out"

capture ./show-app one two
check "a built program, ./app one two" "$TWO_ARGS" "$out"

echo
echo "With no arguments only the program is counted"

capture "$BIN" run show.krb
check "krb run file" "$NO_ARGS" "$out"

capture bash -c "cd proj && '$BIN' run"
check "krb run (the project's bin)" "$NO_ARGS" "$out"

capture "$BIN" exec "$SHOW_CODE"
check "krb exec '<code>'" "$NO_ARGS" "$out"

capture ./show-app
check "a built program, ./app" "$NO_ARGS" "$out"

echo
echo "A built program sees what krb run shows"

capture "$BIN" run show.krb a b c
run_out="$out"
capture ./show-app a b c
check "same output for 'a b c'" "$run_out" "$out"

echo
echo "Awkward arguments arrive as they were typed"

capture "$BIN" run show.krb "two words" ""
check "a space and an empty argument" $'3\ntwo words\n\nnil' "$out"

capture "$BIN" run show.krb --help -v
check "flags are the script's, not krb's" $'3\n--help\n-v\nnil' "$out"

capture ./show-app "two words" ""
check "a built program, a space and an empty argument" $'3\ntwo words\n\nnil' "$out"

echo
echo "@argv(0) is the program, as it was typed"

capture "$BIN" run ./zero.krb
check "krb run ./zero.krb" "./zero.krb" "$out"

capture "$BIN" run zero.krb
check "krb run zero.krb" "zero.krb" "$out"

capture bash -c "cd proj && '$BIN' example zero"
check "krb example zero" "examples/zero.krb" "$out"

capture "$BIN" exec '@println(@argv(0));'
check "krb exec" "exec" "$out"

capture ./zero-app
check "a built program" "./zero-app" "$out"

echo
echo "The REPL"

repl_out=$(printf '@println(@argc());\n@println(@argv(0));\n' | "$BIN" repl 2>/dev/null)
check "counts only itself" "1" "$(grep -x '1' <<<"$repl_out" | head -n 1)"
check "is called repl" "repl" "$(grep -x 'repl' <<<"$repl_out" | head -n 1)"

echo
echo "Total: $((PASS + FAIL)) | Passed: $PASS | Failed: $FAIL"

[[ $FAIL -eq 0 ]]
