#!/usr/bin/env bash
# End-to-end test of seqc_legacy: from `new` to a running ELF executable.
#
# Usage: end_to_end.sh /path/to/seqc_legacy

set -u

if [ $# -ne 1 ]; then
  echo "usage: $0 /path/to/seqc_legacy" >&2
  exit 2
fi
SEQC=$(realpath "$1")
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK" || exit 1

failures=0

fail() {
  echo "FAIL: $*"
  failures=$((failures + 1))
}

# expect_eq <what> <expected> <actual>
expect_eq() {
  if [ "$2" != "$3" ]; then
    fail "$1"
    echo "--- expected"
    echo "$2"
    echo "--- actual"
    echo "$3"
    echo "---"
  fi
}

# expect_contains <what> <needle> <haystack>
expect_contains() {
  case "$3" in
    *"$2"*) ;;
    *)
      fail "$1: missing '$2'"
      echo "--- actual"
      echo "$3"
      echo "---"
      ;;
  esac
}

# set_program <rows> <top>: rewrites src/main.seq with the two numbers.
set_program() {
  cat > src/main.seq <<EOF
name = "top3Company"

step step1():
    ask("create file company.txt with $1 sample-company-transaction-db")

step step2():
    ask("for-each transaction get $2 top total-revenue-by-company")
EOF
}

# --- seqc_legacy new ---------------------------------------------------------

out=$("$SEQC" new top3Company)
expect_eq "new: exit status" 0 $?
expect_eq "new: console output" "Seq Legacy Compiler

Creating project: top3Company

[create] top3Company/
[create] top3Company/src/
[create] top3Company/src/main.seq
[create] top3Company/output/

Target: Linux x86_64

Project created successfully.

Next:
  cd top3Company
  seqc_legacy src/main.seq" "$out"
expect_eq "new: tree" "top3Company
top3Company/output
top3Company/src
top3Company/src/main.seq" "$(find top3Company | sort)"
expect_eq "new: starter program" 'name = "top3Company"

step step1():
    ask("create file company.txt with 5 sample-company-transaction-db")

step step2():
    ask("for-each transaction get 3 top total-revenue-by-company")' \
  "$(cat top3Company/src/main.seq)"

"$SEQC" new top3Company > /dev/null 2>&1
expect_eq "new: existing project is refused" 7 $?
"$SEQC" new 9lives > /dev/null 2>&1
expect_eq "new: invalid name" 2 $?
"$SEQC" frobnicate > /dev/null 2>&1
expect_eq "unknown command" 2 $?
"$SEQC" --no-such-option > /dev/null 2>&1
expect_eq "unknown option" 2 $?

# --- The reference program: 5 rows, top 3 ------------------------------------

cd top3Company || exit 1

BUILD_OUTPUT="Seq Legacy Compiler

[lex] src/main.seq
[parse] 1 name, 2 steps
[sema] 2 requests
[ir] output/temp/top3Company.ir
[codegen] output/temp/top3Company.asm
[assemble] output/temp/top3Company.lst
[link] output/temp/top3Company.bin
[run] top3Company.bin
Top 3 total-revenue-by-company:
1. Globex 480.00
2. ACME 385.00
3. Initech 292.50
[run] exit 0

Results:
  output/company.txt"

FIVE_ROWS="ACME,10,25.50
Globex,4,120.00
Initech,30,9.75
ACME,5,26.00
Umbrella,2,40.00"

out=$("$SEQC" src/main.seq)
expect_eq "5/3: exit status" 0 $?
expect_eq "5/3: console output" "$BUILD_OUTPUT" "$out"
expect_eq "5/3: company.txt" "$FIVE_ROWS" "$(cat output/company.txt)"
expect_eq "5/3: output tree" "output
output/company.txt
output/temp
output/temp/top3Company.asm
output/temp/top3Company.ast
output/temp/top3Company.bin
output/temp/top3Company.ir
output/temp/top3Company.lst
output/temp/top3Company.tokens" "$(find output | sort)"
[ -x output/temp/top3Company.bin ] || fail "5/3: the binary is not executable"
expect_eq "5/3: ELF magic" "7f454c46" \
  "$(head -c 4 output/temp/top3Company.bin | od -An -tx1 | tr -d ' \n')"

if command -v file > /dev/null && command -v readelf > /dev/null; then
  described=$(file output/temp/top3Company.bin)
  expect_contains "file" "ELF 64-bit LSB executable, x86-64" "$described"
  expect_contains "file" "statically linked" "$described"
  headers=$(readelf -h -l output/temp/top3Company.bin 2>&1)
  expect_eq "readelf: exit status" 0 $?
  expect_contains "readelf" "Entry point address:               0x400080" \
    "$headers"
  case "$headers" in
    *[Ww]arning* | *[Ee]rror*) fail "readelf reported a problem: $headers" ;;
  esac
fi

# The binary stands on its own: run it directly, without seqc_legacy.
rm output/company.txt
out=$(cd output && ./temp/top3Company.bin)
expect_eq "direct run: exit status" 0 $?
expect_eq "direct run: console output" "Top 3 total-revenue-by-company:
1. Globex 480.00
2. ACME 385.00
3. Initech 292.50" "$out"
expect_eq "direct run: company.txt" "$FIVE_ROWS" "$(cat output/company.txt)"

# --- No external tool: the same build with an empty PATH ---------------------

rm -rf output
out=$(/usr/bin/env -i PATH= "$SEQC" src/main.seq)
expect_eq "empty PATH: exit status" 0 $?
expect_eq "empty PATH: console output" "$BUILD_OUTPUT" "$out"
expect_eq "empty PATH: company.txt" "$FIVE_ROWS" "$(cat output/company.txt)"

# --- 5 rows, top 2 -----------------------------------------------------------

set_program 5 2
out=$("$SEQC" src/main.seq)
expect_eq "5/2: exit status" 0 $?
expect_contains "5/2" "[run] top3Company.bin
Top 2 total-revenue-by-company:
1. Globex 480.00
2. ACME 385.00
[run] exit 0" "$out"
expect_eq "5/2: company.txt" "$FIVE_ROWS" "$(cat output/company.txt)"

# --- 10 rows, top 3 ----------------------------------------------------------

set_program 10 3
out=$("$SEQC" src/main.seq)
expect_eq "10/3: exit status" 0 $?
expect_contains "10/3" "[run] top3Company.bin
Top 3 total-revenue-by-company:
1. Globex 717.00
2. Umbrella 698.00
3. Hooli 501.00
[run] exit 0" "$out"
expect_eq "10/3: company.txt" "$FIVE_ROWS
Globex,2,118.50
Hooli,12,33.25
Initech,8,10.00
Umbrella,15,41.20
Hooli,3,34.00" "$(cat output/company.txt)"

# --- Every row ---------------------------------------------------------------

set_program 500 100
"$SEQC" src/main.seq > /dev/null
expect_eq "500/100: exit status" 0 $?
expect_eq "500/100: rows" 500 "$(wc -l < output/company.txt)"

# --- check and --build-only --------------------------------------------------

set_program 5 3
out=$("$SEQC" check src/main.seq)
expect_eq "check: exit status" 0 $?
expect_eq "check: console output" "src/main.seq: ok
  name:  top3Company
  steps: 2 (2 requests)
    1. step1 (1)
    2. step2 (1)" "$out"

rm -rf output
out=$("$SEQC" --build-only src/main.seq)
expect_eq "build-only: exit status" 0 $?
expect_contains "build-only" "[link] output/temp/top3Company.bin" "$out"
case "$out" in
  *"[run]"*) fail "build-only: the program was run" ;;
esac
[ -e output/company.txt ] && fail "build-only: company.txt was created"
[ -x output/temp/top3Company.bin ] || fail "build-only: no binary"

# --- Failures ----------------------------------------------------------------

# A source error: exit 3, the diagnostic on standard error, nothing built.
rm -rf output
cat > src/main.seq <<'EOF'
name = "top3Company"

step step1():
    ask("make me a sandwich")
EOF
err=$("$SEQC" src/main.seq 2>&1 > /dev/null)
expect_eq "source error: exit status" 3 $?
expect_eq "source error: diagnostics" 'src/main.seq:4:9: error[E0401]: unrecognized request
        ask("make me a sandwich")
            ^
    hint: supported: create file <file> with <N> <dataset>
seqc_legacy: 1 error(s) in src/main.seq' "$err"
[ -e output ] && fail "source error: output/ was created"
"$SEQC" check src/main.seq > /dev/null 2>&1
expect_eq "check: source error" 3 $?

# The generated program fails when it cannot create its file: exit 70 from
# the program, exit 6 from seqc_legacy.
set_program 5 3
mkdir -p output/company.txt
out=$("$SEQC" src/main.seq 2> /dev/null)
expect_eq "program failure: exit status" 6 $?
expect_contains "program failure" "[run] exit 70" "$out"
rmdir output/company.txt

# Only <project>/src/main.seq is a source file.
cp src/main.seq other.seq
"$SEQC" other.seq > /dev/null 2>&1
expect_eq "wrong source location" 2 $?
"$SEQC" src/missing/../main.seq > /dev/null 2>&1
expect_eq "normalized source path" 0 $?
rm src/main.seq
"$SEQC" src/main.seq > /dev/null 2>&1
expect_eq "missing source file" 7 $?

if [ "$failures" -ne 0 ]; then
  echo "$failures check(s) failed"
  exit 1
fi
echo "end_to_end: all checks passed"
