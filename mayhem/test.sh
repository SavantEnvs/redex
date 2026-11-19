#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

B="$SRC/mayhem/build"
for t in position_map_reader_test resource_selftest; do
  [ -x "$B/$t" ] || { echo "missing $B/$t: run mayhem/build.sh first" >&2; emit_ctrf "gtest+golden" 0 1; exit 1; }
done
passed=0; failed=0

# 1) Upstream's own line-map reader tests (test/unit/PositionMapReaderTest.cpp, gtest).
#    Count the "[       OK ]" / "[  FAILED  ]" lines; a run that reports no tests is a failure.
log=$("$B/position_map_reader_test" 2>/dev/null); rc=$?
ok=$(grep -c '^\[       OK \]' <<<"$log" || true)
bad=$(grep -c '^\[  FAILED  \] [A-Za-z]' <<<"$log" || true)
echo "PositionMapReaderTest: $ok passed, $bad failed (exit $rc)"
passed=$((passed + ok)); failed=$((failed + bad))
if [ "$ok" -eq 0 ] || { [ "$rc" -ne 0 ] && [ "$bad" -eq 0 ]; }; then failed=$((failed + 1)); fi

# 2) Known-answer check of libresource's arsc visitor and binary-XML parser on upstream's
#    fixtures: the output must match the committed golden file line for line.
inputs=(test/unit/example_resources.arsc test/unit/resources_unknown_chunk.arsc
        test/unit/example_layout.xml test/unit/example_layout_with_onclick.xml
        test/integ/ClinitBatchingTestManifest.xml)
actual=$("$B/resource_selftest" "${inputs[@]}" 2>/dev/null)
i=0
while IFS= read -r want; do
  i=$((i + 1))
  got=$(sed -n "${i}p" <<<"$actual")
  if [ "$got" = "$want" ]; then passed=$((passed + 1)); else
    failed=$((failed + 1)); echo "FAIL resource_selftest line $i"; echo "  want: $want"; echo "  got:  $got"; fi
done < "$SRC/mayhem/tests/resource_selftest.expected"

emit_ctrf "gtest+golden" "$passed" "$failed"
