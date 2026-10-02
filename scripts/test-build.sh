#!/usr/bin/env bash

# Tests of 'krb build' itself: its arguments, its errors, and the programs it
# makes. That a built program behaves like 'krb run' for every test in tests/ is
# checked by './scripts/tests.sh --packaged'.

set -uo pipefail

BIN="${BIN:-./build/krb}"
RUNTIME_BIN="${RUNTIME_BIN:-./build/krb-runtime}"

BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
RUNTIME_BIN="$(cd "$(dirname "$RUNTIME_BIN")" && pwd)/$(basename "$RUNTIME_BIN")"

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
        echo "     expected: $expected"
        echo "     actual:   $actual"
    fi
}

check_contains() {
    local name="$1"
    local needle="$2"
    local haystack="$3"

    if [[ "$haystack" == *"$needle"* ]]; then
        PASS=$((PASS + 1))
        echo "  ✅ $name"
    else
        FAIL=$((FAIL + 1))
        echo "  ❌ $name"
        echo "     expected to contain: $needle"
        echo "     actual:              $haystack"
    fi
}

# Runs a command and sets $out (stdout), $err (stderr) and $code (exit code).
capture() {
    local out_file="$WORK_DIR/stdout"
    local err_file="$WORK_DIR/stderr"

    "$@" >"$out_file" 2>"$err_file" </dev/null
    code=$?
    out=$(<"$out_file")
    err=$(<"$err_file")
}

cd "$WORK_DIR"

cat >hello.krb <<'EOF'
fun greet(name: string): string {
  return $"Hello, {name}!";
}

@println(greet("packaged world"));
EOF

cat >args.krb <<'EOF'
@println(@argc());
@println(@argv(1));
@println(@argv(2));
EOF

cat >boom.krb <<'EOF'
@println("before");
@println(@len(1));
@println("after");
EOF

cat >leave.krb <<'EOF'
@exit(3);
EOF

printf 'let x: number = "not a number";\n' >broken.krb

echo "A built program"

capture "$BIN" build hello.krb -o hello
check "builds" "0" "$code"
check_contains "says what it built" "Built hello" "$err"

capture ./hello
check "runs" "0" "$code"
check "prints what the script prints" "Hello, packaged world!" "$out"

capture "$BIN" run hello.krb
check "prints what 'krb run' prints" "Hello, packaged world!" "$out"

mkdir elsewhere
cp hello elsewhere/moved
capture bash -c 'cd elsewhere && PATH=/usr/bin:/bin ./moved'
check "runs after being moved, without krb" "Hello, packaged world!" "$out"

capture "$BIN" build args.krb -o args
capture ./args one "two words"
check "gets its arguments after its own name" $'3\none\ntwo words' "$out"

capture "$BIN" build boom.krb -o boom
capture ./boom
check "stops with the runtime error exit code" "70" "$code"
check "prints what ran before the error" "before" "$out"
check_contains "reports the error with its line" "[line 2] in script" "$err"

capture "$BIN" build leave.krb -o leave
capture ./leave
check "keeps the exit code from @exit" "3" "$code"

echo
echo "Building over a program that exists"

capture "$BIN" build args.krb -o hello
capture ./hello a
check "replaces it" $'2\na\nnil' "$out"

echo
echo "Choosing the runtime"

mkdir runtimes
cp "$RUNTIME_BIN" runtimes/other-runtime
capture "$BIN" build hello.krb -o with-flag --runtime runtimes/other-runtime
capture ./with-flag
check "uses the runtime given with --runtime" "Hello, packaged world!" "$out"

capture "$BIN" build hello.krb -o missing --runtime runtimes/nope
check "fails when the runtime is missing" "71" "$code"
check_contains "says which runtime" "runtimes/nope" "$err"
check "leaves no output" "no" "$([[ -e missing ]] && echo yes || echo no)"

cp "$RUNTIME_BIN" same-file
before=$(cksum <same-file)
capture "$BIN" build hello.krb -o same-file --runtime same-file
check "refuses to overwrite its own runtime" "71" "$code"
check "leaves the runtime alone" "$before" "$(cksum <same-file)"

echo
echo "A build that fails"

capture "$BIN" build broken.krb -o broken
check "exits with the compile error code" "65" "$code"
check "leaves no output" "no" "$([[ -e broken ]] && echo yes || echo no)"

capture "$BIN" build nope.krb -o nope
check "exits with the OS error code when the file is missing" "71" "$code"
check_contains "says which file" "nope.krb" "$err"
check "leaves no output" "no" "$([[ -e nope ]] && echo yes || echo no)"

echo
echo "Wrong arguments"

capture "$BIN" build
check "no arguments" "64" "$code"

capture "$BIN" build hello.krb
check "no -o" "64" "$code"

capture "$BIN" build -o app
check "no path" "64" "$code"

capture "$BIN" build hello.krb -o
check "-o without a name" "64" "$code"

capture "$BIN" build hello.krb extra.krb -o app
check "two paths" "64" "$code"

capture "$BIN" build hello.krb -o app --nonsense
check "an unknown option" "64" "$code"

echo
echo "The runtime on its own"

capture "$RUNTIME_BIN"
check "has no program to run" "64" "$code"
check_contains "says how to make one" "krb build" "$err"

echo
echo "A damaged program"

capture "$BIN" build hello.krb -o damaged-base
size=$(wc -c <damaged-base)
runtime_size=$(wc -c <"$RUNTIME_BIN")

cp damaged-base wrong-length
printf '\177' | dd of=wrong-length bs=1 seek=$((size - 1)) conv=notrunc 2>/dev/null
capture ./wrong-length
check "a payload length past the file" "71" "$code"
check_contains "is called damaged" "damaged" "$err"

# The first unit starts 4 bytes after the runtime: its length, then "KRBC", a
# 2 byte format version, and the Kirby version's length and text.
cp damaged-base wrong-version
printf 'X' | dd of=wrong-version bs=1 seek=$((runtime_size + 4 + 7)) conv=notrunc 2>/dev/null
capture ./wrong-version
check "a program from another Kirby version" "71" "$code"
check_contains "says so" "different version of Kirby" "$err"

head -c $((size - 5)) damaged-base >cut-short
chmod +x cut-short
capture ./cut-short
check "a cut off trailer" "64" "$code"

echo
echo "Total: $((PASS + FAIL)) | Passed: $PASS | Failed: $FAIL"

[[ $FAIL -eq 0 ]]
