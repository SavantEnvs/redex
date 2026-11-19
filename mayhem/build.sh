#!/usr/bin/env bash
#
# mayhem/build.sh — builds redex's dex reader (libredex), libresource (the resources.arsc / binary XML
# parsers) and the line-map reader (tools/debug-info/PositionMap.cpp) with the
# sanitizers, links the libFuzzer harnesses + their -standalone
# reproducers, and builds the test oracle (upstream's PositionMapReaderTest
# gtest + a libresource known-answer check) with normal flags.
# Outputs go to mayhem/build/ (gitignored by mayhem/.gitignore). Offline and
# re-runnable: it only reads the tree and apt-installed headers/libs.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/mayhem/build"
mkdir -p "$OUT/fuzz-obj" "$OUT/test-obj"

INCS=(-I"$SRC/libresource" -I"$SRC/tools/debug-info")
CXXSTD=(-std=c++20 -pthread)

# compile <objdir> <flags...> -- <sources...>   (parallel, one object per source)
compile() {
  local objdir="$1"; shift
  local flags=()
  while [ "$1" != "--" ]; do flags+=("$1"); shift; done; shift
  local pids=() src obj
  for src in "$@"; do
    obj="$objdir/$(echo "${src#"$SRC"/}" | tr '/' '_').o"
    "$CXX" "${CXXSTD[@]}" "${flags[@]}" "${INCS[@]}" -c "$src" -o "$obj" &
    pids+=($!)
    if [ "${#pids[@]}" -ge "$MAYHEM_JOBS" ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  done
  local p rc=0
  for p in "${pids[@]}"; do wait "$p" || rc=1; done
  return $rc
}

RES_SRCS=("$SRC"/libresource/*.cpp)
LINEMAP_SRCS=("$SRC/tools/debug-info/PositionMap.cpp")

# 1) Sanitized + instrumented project code (the fuzzed code) and harnesses.
# UBSan's `alignment` check is relaxed (ASan and the rest of UBSan stay halting):
# libresource and ApkResources cast chunk structs at offsets read from the file,
# which redex maps with no alignment guarantee (as Android's own androidfw does),
# so every misaligned chunk produced a separate "misaligned address" defect that
# is benign on x86-64/arm64 and buried the real out-of-bounds reads.
FFLAGS=(-O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link)
[ -z "$SANITIZER_FLAGS" ] || FFLAGS+=(-fno-sanitize=alignment)
rm -f "$OUT"/fuzz-obj/*.o
compile "$OUT/fuzz-obj" "${FFLAGS[@]}" -- "${RES_SRCS[@]}" "${LINEMAP_SRCS[@]}" \
  "$SRC"/mayhem/harnesses/fuzz_{arsc_visitor,xml_parser,linemap_reader}.cpp "$SRC/mayhem/lsan_off.cc"
rm -f "$OUT/libresource_fuzz.a"
ar rcs "$OUT/libresource_fuzz.a" "$OUT"/fuzz-obj/libresource_*.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

H="$OUT/fuzz-obj/mayhem_harnesses_fuzz"
LSAN="$OUT/fuzz-obj/mayhem_lsan_off.cc.o"
LM="$OUT/fuzz-obj/tools_debug-info_PositionMap.cpp.o"
link_target() {  # name objs...
  local name="$1"; shift
  "$CXX" "${CXXSTD[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$@" "$LSAN" -o "$OUT/$name"
  "$CXX" "${CXXSTD[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$@" "$LSAN" -o "$OUT/$name-standalone"
}
link_target fuzz_arsc_visitor "${H}_arsc_visitor.cpp.o" "$OUT/libresource_fuzz.a"
link_target fuzz_xml_parser "${H}_xml_parser.cpp.o" "$OUT/libresource_fuzz.a"
link_target fuzz_linemap_reader "${H}_linemap_reader.cpp.o" "$LM"

# 1b) The part of libredex the dex reader needs (libredex/ + the shared/util/liblocator
#     files upstream's CMake `redex` library lists), sanitized + coverage-instrumented.
#     upstream's opt/ and service/ passes are not compiled: load_classes_from_dex() does
#     not reach them, and building all ~420 TUs of the CMake target took the CI image
#     build past 30 minutes. Include dirs mirror upstream's CMakeLists.txt glob.
REDEX_INCS=(-I"$SRC/analysis" -I"$SRC/libredex" -I"$SRC/shared" -I"$SRC/util" -I"$SRC/sparta/include"
            -I"$SRC/liblocator" -I"$SRC/libresource" -I"$SRC/tools/common" -I/usr/include/jsoncpp)
for d in "$SRC"/service/* "$SRC"/opt/*; do [ -d "$d" ] && REDEX_INCS+=(-I"$d"); done
REDEX_SRCS=()
for f in "$SRC"/libredex/*.cpp; do
  [ "$(basename "$f")" = ProtoReflection.cpp ] || REDEX_SRCS+=("$f")  # CMake drops it too
done
REDEX_SRCS+=("$SRC"/shared/{DexDefs,DexEncoding,file-utils}.cpp
             "$SRC"/util/{CommandProfiling,Histogram,JemallocUtil,Sha1}.cpp
             "$SRC/liblocator/locator.cpp")
mkdir -p "$OUT/redex-obj"
compile "$OUT/redex-obj" "${FFLAGS[@]}" "${REDEX_INCS[@]}" -- "${REDEX_SRCS[@]}"
REDEX_OBJS=()
for f in "${REDEX_SRCS[@]}"; do REDEX_OBJS+=("$OUT/redex-obj/$(echo "${f#"$SRC"/}" | tr '/' '_').o"); done
rm -f "$OUT/libredex_fuzz.a"
ar rcs "$OUT/libredex_fuzz.a" "${REDEX_OBJS[@]}"
REDEX_LIBS=("$OUT/libredex_fuzz.a" -lboost_filesystem -lboost_iostreams -lboost_program_options
            -lboost_regex -lboost_thread -ljsoncpp -lz)
REDEX_HARNESSES=(dex_loader jar_loader class_loader proguard_parser proguard_map apk_resources)
HSRCS=()
for h in "${REDEX_HARNESSES[@]}"; do HSRCS+=("$SRC/mayhem/harnesses/fuzz_$h.cpp"); done
compile "$OUT/redex-obj" "${FFLAGS[@]}" "${REDEX_INCS[@]}" -- "${HSRCS[@]}"
for h in "${REDEX_HARNESSES[@]}"; do
  link_target "fuzz_$h" "$OUT/redex-obj/mayhem_harnesses_fuzz_$h.cpp.o" "${REDEX_LIBS[@]}" \
    "$OUT/libresource_fuzz.a"
done

# 2) The test oracle, with normal flags (no sanitizers): upstream's
#    PositionMapReaderTest (gtest) and the libresource known-answer check.
TFLAGS=(-O1 $COVERAGE_FLAGS)
rm -f "$OUT"/test-obj/*.o
compile "$OUT/test-obj" "${TFLAGS[@]}" -- "${RES_SRCS[@]}" "${LINEMAP_SRCS[@]}" \
  "$SRC/test/unit/PositionMapReaderTest.cpp" "$SRC/mayhem/tests/resource_selftest.cpp"
"$CXX" "${CXXSTD[@]}" "${TFLAGS[@]}" "$OUT/test-obj/test_unit_PositionMapReaderTest.cpp.o" \
  "$OUT/test-obj/tools_debug-info_PositionMap.cpp.o" -lgtest -lgtest_main -o "$OUT/position_map_reader_test"
"$CXX" "${CXXSTD[@]}" "${TFLAGS[@]}" "$OUT/test-obj/mayhem_tests_resource_selftest.cpp.o" \
  "$OUT"/test-obj/libresource_*.o -o "$OUT/resource_selftest"

ls -l "$OUT"/fuzz_* "$OUT/position_map_reader_test" "$OUT/resource_selftest"
